"""Focused authoring guards; expected rules come from the pinned Oracle packet."""
import copy,json,unittest
from pathlib import Path
from generate_cases import registry
from token_trigger_creation_recipes import expand
from token_trigger_creation_spec import spec,variants,validate_witnesses,creation

class TokenTriggerCreationTest(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.oracle=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards']
  cls.rows={r['name']:r for r in registry()}
 def declaration(self,name='Doomed Traveler'):
  r=self.oracle[name];text=r['oracle_text']
  if name in ('Ant Queen','Jade Mage','Centaur\'s Herald','Sliver Queen','Thraben Doomsayer'):mode='activation'
  elif name=='Grave Titan':mode='entry_attack'
  elif name=='Mardu Strike Leader':mode='attack'
  elif name in ('Talrand, Sky Summoner','Young Pyromancer','Murmuring Mystic'):mode='own_spell'
  elif name=='Dragonlair Spider':mode='opponent_spell'
  else:mode='death'
  import re
  # Keep full case and subtype spelling from Oracle instead of using token rows.
  clause=re.search(r'[Cc]reate .+?creature tokens?(?: with [a-z ,]+)?\.',text)[0];token=creation(clause[0].upper()+clause[1:])
  activation=None
  if mode=='activation':
   from recipes import cost_from_oracle
   m=re.match(r'((?:\{[0-9WUBRGC]+\})*)(, \{T\})?(, Sacrifice this creature)?: ',text)
   activation=dict(mana=[0]*6,tap=True,sacrifice=False) if text.startswith('{T}: ') else dict(mana=cost_from_oracle(dict(mana_cost=m[1])),tap=bool(m[2]),sacrifice=bool(m[3]))
  from token_trigger_creation_spec import INACTIVE
  return dict(family='token_trigger_creation',version=2,params=dict(mode=mode,keywords=sorted(k for k in r['keywords'] if k not in ('Afterlife','Dash','Fateful hour')),token=token,activation=activation,inactive_clauses=INACTIVE.get(name,[])),oracle_text=text,variants=['create_0'])
 def entry_declaration(self,name='Cloudgoat Ranger'):
  import re
  from token_trigger_creation_spec import entry_profiles,component_creation,STATIC_ENTRY_CLAUSES,GRANT_ENTRY_CLAUSES
  profile=entry_profiles()[name];r=self.oracle[name];clause=profile['entry_clause'];fabricate=int(clause.split()[1]) if profile['mode']=='fabricate' else 0
  body=re.search(r'create .+?creature tokens?(?: with [a-z ,]+)?\.',clause)[0] if fabricate else clause.split(': ',1)[1] if profile['mode']=='activation' else clause.split(', ',1)[1];body=body.removeprefix('you may ') if profile['mode']=='landfall' else body;token,mana=component_creation(body)
  params=dict(profile=profile.get('profile','entry_component'),keywords=sorted(k for k in r['keywords'] if k not in ('Fabricate','Devoid') and (k!='Landfall' or profile['mode']!='landfall') and k not in profile.get('inactive_keywords',[])),token=token,inactive_clauses=profile['inactive_clauses'],fabricate=fabricate)
  if profile.get('profile')=='event_component':
   from recipes import cost_from_oracle
   params.pop('fabricate');m=re.fullmatch(r'((?:\{[0-9WUBRG]+\})*)(, \{T\})?: (Create .+)',clause);params.update(activation=dict(mana=cost_from_oracle(dict(mana_cost=m[1])),tap=bool(m[2]),sacrifice=False) if m else None,token_mana=mana)
  if profile['mode']=='landfall':params['optional']='you may create' in clause
  if profile.get('profile')=='followup_component':params['followup']='artifact_loss'
  if profile.get('owner_return_clause'):params['owner_return']=True
  if profile.get('active_static_clause'):params['active_static']=copy.deepcopy((GRANT_ENTRY_CLAUSES if profile.get('profile')=='grant_component' else STATIC_ENTRY_CLAUSES)[profile['active_static_clause']])
  if profile.get('token_mana'):params['token_mana']=True
  if profile.get('devoid_clause'):params['devoid']=True
  return dict(family='token_trigger_creation',version=2,params=params,oracle_text=r['oracle_text'],variants=['create_0'])
 def entry_cases(self,name='Cloudgoat Ranger',selected=None,d=None):
  return self.cases(name,selected,d=d or self.entry_declaration(name))
 def cases(self,name='Doomed Traveler',selected=None,d=None,o=None,rows=None):
  d=d or self.declaration(name);p=spec(d,self.oracle[name]);selected=selected or d['variants'];allowed=variants(p)
  if not selected or len(set(selected))!=len(selected) or any(v not in allowed for v in selected):raise ValueError('unique reviewed variants required')
  validate_witnesses(o or self.oracle,rows or self.rows,p)
  return expand(name,self.rows[name]['csvid'],d,p,selected,rows or self.rows,o or self.oracle)
 def has(self,case,op,**kwargs):self.assertIn(dict(op=op,**kwargs),case['steps'])
 def test_entry_component_real_cast_and_inactive_activation_pin(self):
  c=self.entry_cases()[0];self.has(c,'cast',ref='source',value=1);self.has(c,'assert_zone_multiplicity',player=0,zone='battlefield',index=self.rows['Kithkin Soldier']['csvid'],value=3);self.assertTrue(any('Tap three untapped Kithkin' in gap for gap in c['_recipe']['untested_components']))
 def test_entry_component_unknown_or_active_extra_text_rejects(self):
  r=dict(self.oracle['Cloudgoat Ranger']);r['oracle_text']+='\nWhenever this creature attacks, draw a card.';d=self.entry_declaration();d['oracle_text']=r['oracle_text']
  with self.assertRaises(ValueError):spec(d,r)
  d=self.entry_declaration();d['params']['profile']='unknown'
  with self.assertRaises(ValueError):self.entry_cases(d=d)
 def test_fabricate_token_choice_and_counter_choice_are_separate(self):
  tokens,counters=self.entry_cases('Visionary Augmenter',['create_0','fabricate_counters_1']);self.has(tokens,'choose_dialog',player=0,value=1);self.has(tokens,'assert_zone_multiplicity',player=0,zone='battlefield',index=self.rows['Servo']['csvid'],value=2);self.has(counters,'choose_dialog',player=1,value=0);self.has(counters,'assert_card',ref='source',property='plus1_counters',value=2);self.has(counters,'assert_card',ref='source',property='power',value=4);self.has(counters,'assert_card',ref='source',property='toughness',value=3)
 def test_fabricate_preloaded_source_is_not_an_unscripted_choice(self):
  d=self.entry_declaration('Visionary Augmenter');self.assertNotIn('preloaded_source_0',variants(spec(d,self.oracle['Visionary Augmenter'])))
 def test_fabricate_replay_queues_choices_for_each_original_controller(self):
  c=self.entry_cases('Visionary Augmenter',['fabricate_counters_snapshot_1'])[0];steps=c['steps'];restore=steps.index(dict(op='restore'));self.assertIn(dict(op='choose_dialog',player=1,value=0),steps[:restore]);self.assertIn(dict(op='choose_dialog',player=1,value=0),steps[restore:]);self.assertFalse(any(s['op']=='choose_dialog' and s.get('player')==0 for s in steps))
 def test_entry_blink_and_source_loss_keep_complete_token_receipts(self):
  blink,removed=self.entry_cases('Whirler Rogue',['blink_0','source_destroyed_1']);self.has(blink,'assert_zone_multiplicity',player=0,zone='battlefield',index=self.rows['Thopter']['csvid'],value=4);self.has(removed,'assert_zone_multiplicity',player=1,zone='battlefield',index=self.rows['Thopter']['csvid'],value=2)
 def test_entry_component_exact_generic_reserve_is_explicit(self):
  c=self.entry_cases()[0];self.has(c,'set_mana',player=0,color=0,value=11);self.has(c,'assert_mana',player=0,color=0,value=8)
 def test_static_golem_bonus_is_active_and_source_loss_restores_base(self):
  create,lost,stacked=self.entry_cases('Master Splicer',['create_0','source_destroyed_1','two_casts_0']);tid=self.rows['Golem']['csvid']
  self.has(create,'assert_matching_permanents',player=0,index=tid,property='power',value=4);self.has(lost,'assert_matching_permanents',player=1,index=tid,property='power',value=3);self.has(stacked,'assert_matching_permanents',player=0,index=tid,property='power',value=5);self.has(create,'assert_matching_permanents',player=0,index=tid,property='subtype_Phyrexian',value=1);self.has(create,'assert_matching_permanents',player=0,index=tid,property='subtype_Golem',value=1)
 def test_spawn_and_scion_sacrifice_make_real_immediate_colorless_mana(self):
  for name,token in (('Nest Invader','Eldrazi Spawn'),('Carrier Thrall','Eldrazi Scion')):
   c=self.entry_cases(name,['token_mana_0'])[0];self.has(c,'activate',ref='mana_token',value=1);self.has(c,'assert_stack',value=0);self.has(c,'assert_card',ref='mana_token',property='in_play',value=0);self.has(c,'assert_zone_multiplicity',player=0,zone='battlefield',index=self.rows[token]['csvid'],value=0);self.has(c,'assert_mana',player=0,color=0,value=10)
 def test_token_mana_snapshot_replays_both_real_activations(self):
  c=self.entry_cases('Carrier Thrall',['token_mana_snapshot_1'])[0];steps=c['steps'];restore=steps.index(dict(op='restore'));self.assertIn(dict(op='activate',ref='mana_token',value=1),steps[:restore]);self.assertIn(dict(op='activate',ref='mana_token',value=1),steps[restore:]);self.has(c,'assert_recorded_state')
 def test_zero_power_token_extension_does_not_loosen_legacy_parser(self):
  clause='Create a 0/1 colorless Eldrazi Spawn creature token.'
  with self.assertRaises(ValueError):creation(clause)
  self.assertEqual(creation(clause,extended=True)['power'],0)
 def test_token_mana_clause_and_static_effect_cannot_be_claimed_as_gaps(self):
  for name,key in (('Carrier Thrall','token_mana'),('Master Splicer','active_static')):
   d=self.entry_declaration(name);d['params'].pop(key)
   with self.assertRaises(ValueError):self.entry_cases(name,d=d)
 def test_all_final_entry_profiles_expand_supported_variants(self):
  from token_trigger_creation_spec import entry_profiles
  for name in entry_profiles():
   d=self.entry_declaration(name);p=spec(d,self.oracle[name]);cases=self.entry_cases(name,variants(p));self.assertTrue(cases)
 def test_devoid_is_an_observed_source_property_not_an_inactive_gap(self):
  c=self.entry_cases('Eldrazi Skyspawner')[0];self.has(c,'assert_matching_permanents',player=0,index=self.rows['Eldrazi Skyspawner']['csvid'],property='color',value=0)
  d=self.entry_declaration('Eldrazi Skyspawner');d['params'].pop('devoid')
  with self.assertRaises(ValueError):self.entry_cases('Eldrazi Skyspawner',d=d)
 def test_many_scion_mana_activations_conserve_count_and_exact_mana(self):
  c=self.entry_cases('Brood Monitor',['token_mana_all_0'])[0];self.assertEqual(sum(s['op']=='activate' and s.get('ref')=='mana_token' for s in c['steps']),3);self.has(c,'assert_zone_multiplicity',player=0,zone='battlefield',index=self.rows['Eldrazi Scion']['csvid'],value=0);self.has(c,'assert_mana',player=0,color=0,value=12)
 def test_legendary_entry_sources_omit_duplicate_legend_fixture(self):
  for name in ('Pia Nalaar','Pia and Kiran Nalaar','Breya, Etherium Shaper'):
   d=self.entry_declaration(name);self.assertNotIn('two_casts_0',variants(spec(d,self.oracle[name])));self.assertTrue(self.entry_cases(name))
 def test_large_generic_cast_reserve_stays_nonnegative_after_two_casts(self):
  c=self.entry_cases('Accomplished Automaton',['two_casts_1'])[0];self.has(c,'set_mana',player=1,color=0,value=19);self.has(c,'assert_mana',player=1,color=0,value=5)
 def test_token_mana_grant_reminder_and_token_flying_reminder_are_complete(self):
  self.assertTrue(self.entry_cases('Scion Summoner'));c=self.entry_cases('Ghirapur Gearcrafter')[0];self.has(c,'assert_zone_multiplicity',player=0,zone='battlefield',index=self.rows['Thopter']['csvid'],value=1)
 def test_flying_and_first_strike_grants_have_functional_loss_controls(self):
  fly,fly_lost=self.entry_cases('Wing Splicer',['block_flying_0','grant_lost_0']);self.has(fly,'block',ref='token',other='witness',value=1);self.has(fly_lost,'block',ref='token',other='witness',value=0)
  first,lost=self.entry_cases('Blade Splicer',['block_ground_1','grant_lost_1']);self.has(first,'assert_card',ref='expected_token_0',property='damage',value=0);self.has(lost,'assert_card',ref='expected_token_0',property='damage',value=2)
 def test_trample_vigilance_and_haste_loss_are_real_combat_controls(self):
  trample,lost=self.entry_cases('Maul Splicer',['trample_0','grant_lost_0']);self.has(trample,'assert_life',player=1,value=19);self.has(lost,'assert_life',player=1,value=20)
  vigilant,lost=self.entry_cases('Sensor Splicer',['ready_attack_0','grant_lost_0']);self.has(vigilant,'assert_card',ref='expected_token_0',property='tapped',value=0);self.has(lost,'assert_card',ref='expected_token_0',property='tapped',value=1)
  haste,lost=self.entry_cases('Thopter Engineer',['fresh_attack_0','grant_lost_0']);self.has(haste,'attack',ref='token',value=1);self.has(lost,'attack',ref='token',value=0)
 def test_haste_scope_has_own_artifact_ordinary_and_opponent_controls(self):
  c=self.entry_cases('Thopter Engineer',['haste_scope_1'])[0]
  for ref,value in [('scope_token',1),('own_artifact',1),('other',0),('opponent_artifact',0)]:self.has(c,'attack',ref=ref,value=value)
  self.has(c,'assert_life',player=0,value=18);self.has(c,'assert_mana',player=1,color=0,value=5);self.has(c,'assert_mana',player=0,color=0,value=8)
 def test_haste_scope_snapshot_replays_actual_casts_and_decisions(self):
  c=self.entry_cases('Thopter Engineer',['haste_scope_snapshot_1'])[0];s=c['steps'];cut=s.index(dict(op='restore'))
  for part in (s[:cut],s[cut:]):self.assertIn(dict(op='cast',ref='own_artifact',value=1),part);self.assertIn(dict(op='cast',ref='opponent_artifact',value=1),part)
  self.has(c,'assert_recorded_state')
 def test_angel_bonus_includes_other_creatures_and_excludes_self(self):
  c=self.entry_cases('Angel of Invention')[0];self.has(c,'assert_card',ref='source',property='power',value=2);self.has(c,'assert_card',ref='source',property='toughness',value=1);self.has(c,'assert_card',ref='unselected',property='power',value=3);self.has(c,'assert_matching_permanents',player=0,index=self.rows['Servo']['csvid'],property='power',value=2)
  c=self.entry_cases('Angel of Invention',['two_casts_0'])[0];self.has(c,'assert_card',ref='source',property='power',value=3);self.has(c,'assert_card',ref='second',property='toughness',value=2);self.has(c,'assert_card',ref='unselected',property='power',value=4)
 def test_angel_removal_and_fabricate_choices_keep_mandatory_bonus(self):
  lost,counters=self.entry_cases('Angel of Invention',['source_destroyed_0','fabricate_counters_1']);self.has(lost,'assert_card',ref='unselected',property='power',value=2);self.has(lost,'assert_matching_permanents',player=0,index=self.rows['Servo']['csvid'],property='power',value=1);self.has(counters,'assert_card',ref='source',property='power',value=4);self.has(counters,'assert_card',ref='unselected',property='power',value=3)
 def test_grant_scope_params_cannot_be_silently_changed(self):
  d=self.entry_declaration('Thopter Engineer');d['params']['active_static']['scope']='golem'
  with self.assertRaises(ValueError):self.entry_cases('Thopter Engineer',d=d)
 def test_golem_scopes_compare_token_with_same_board_ordinary_creature(self):
  c=self.entry_cases('Blade Splicer',['grant_scope_0'])[0];self.has(c,'block',ref='scope_token',other='positive_witness',value=1);self.has(c,'block',ref='unselected',other='negative_witness',value=1);self.has(c,'assert_card',ref='unselected',property='in_play',value=0);self.has(c,'assert_card',ref='expected_token_0',property='damage',value=0)
  c=self.entry_cases('Wing Splicer',['grant_scope_1'])[0];self.has(c,'block',ref='unselected',other='negative_witness',value=0);self.has(c,'assert_life',player=1,value=19)
  c=self.entry_cases('Sensor Splicer',['grant_scope_0'])[0];self.has(c,'assert_card',ref='unselected',property='tapped',value=1);self.has(c,'assert_card',ref='expected_token_0',property='tapped',value=0)
  c=self.entry_cases('Maul Splicer',['grant_scope_0'])[0];self.has(c,'assert_life',player=1,value=19);self.has(c,'assert_card',ref='unselected',property='damage',value=1)
 def test_golem_scope_snapshot_replays_complete_combat(self):
  c=self.entry_cases('Blade Splicer',['grant_scope_snapshot_1'])[0];steps=c['steps'];cut=steps.index(dict(op='restore'))
  self.assertIn(dict(op='combat_damage'),steps[:cut]);self.assertIn(dict(op='combat_damage'),steps[cut:]);self.has(c,'assert_recorded_state')
 def test_blinked_angel_source_proofs_follow_the_actual_rebind(self):
  for variant in ('blink_0','blink_snapshot_1'):
   case=self.entry_cases('Angel of Invention',[variant])[0];steps=case['steps']
   for i,a in enumerate(steps):
    if a.get('op')!='cast' or a.get('ref')!='blink':continue
    resolved=next(n for n in range(i+1,len(steps)) if steps[n]['op']=='resolve');bound=next(n for n in range(resolved+1,len(steps)) if steps[n]['op']=='bind_permanent' and steps[n].get('ref')=='source')
    self.assertFalse(any(s['op']=='assert_card' and s.get('ref')=='source' and s.get('property') in ('power','toughness') for s in steps[resolved+1:bound]));self.assertIn(dict(op='assert_card',ref='source',property='power',value=2),steps[bound+1:])
 def test_entry_or_death_adds_tokens_on_each_actual_event(self):
  for name,count in [('Goblin Marshal',2),('Mogg War Marshal',1)]:
   cast,killed,reentered,wrath=self.entry_cases(name,['cast_only_0','create_1','bounce_recast_0','wrath_1'])
   for c,owner,n in [(cast,0,count),(killed,1,2*count),(reentered,0,3*count),(wrath,1,count)]:self.has(c,'assert_zone_multiplicity',player=owner,zone='battlefield',index=self.rows['Goblin']['csvid'],value=n)
   self.has(killed,'destroy',ref='source');self.has(wrath,'cast',ref='wrath',value=1)
 def test_alternative_activation_selects_token_and_keeps_exact_cost(self):
  c=self.entry_cases('Jund Battlemage',['create_1'])[0];self.has(c,'choose_dialog',player=1,value=1);self.has(c,'activate',ref='source',value=1);self.has(c,'assert_mana',player=1,color=3,value=9)
  c=self.entry_cases('Nemata, Grove Guardian',['repeat_activation_0'])[0];self.assertEqual(sum(s==dict(op='choose_dialog',player=0,value=0) for s in c['steps']),2)
 def test_spawn_activation_keeps_real_mana_sacrifice(self):
  c=self.entry_cases('Spawnsire of Ulamog',['token_mana_snapshot_1'])[0];self.has(c,'activate',ref='mana_token',value=1);self.has(c,'assert_recorded_state');self.has(c,'assert_zone_multiplicity',player=1,zone='battlefield',index=self.rows['Eldrazi Spawn']['csvid'],value=1)
 def test_event_component_full_pin_and_cost_reject_mutation(self):
  d=self.entry_declaration('Jund Battlemage');d['params']['activation']['mana'][3]=2
  with self.assertRaises(ValueError):self.entry_cases('Jund Battlemage',d=d)
 def test_captain_bonus_excludes_itself_but_two_captains_share(self):
  c=self.entry_cases('Captain of the Watch',['two_casts_0'])[0];self.has(c,'assert_card',ref='source',property='power',value=4);self.has(c,'assert_card',ref='second',property='toughness',value=4);self.has(c,'assert_card',ref='unselected',property='power',value=2);self.has(c,'assert_matching_permanents',player=0,index=self.rows['Soldier']['csvid'],property='power',value=3)
 def test_spawnsire_opponent_attack_does_not_expect_annihilator_choice(self):
  for owner in (0,1):
   c=self.entry_cases('Spawnsire of Ulamog',['block_ground_'+str(owner)])[0];self.has(c,'choose_target',player=1-owner,ref='witness');self.has(c,'assert_unused_choices',value=1);self.has(c,'attack',ref='witness',value=1);self.assertFalse(any(s['op']=='attack' and s.get('ref')=='source' for s in c['steps']))
 def test_marionette_fabricate3_and_current_power_loss_are_printed(self):
  c=self.entry_cases('Marionette Master',['power_artifact_0'])[0];self.has(c,'choose_dialog',player=0,value=0);self.has(c,'assert_card',ref='source',property='power',value=4);self.has(c,'assert_life',player=1,value=16);self.has(c,'cast',ref='artifact_witness',value=1);self.has(c,'destroy',ref='artifact_witness')
  c=self.entry_cases('Marionette Master',['destroy_token_1'])[0];self.has(c,'assert_life',player=0,value=19)
 def test_marionette_predicate_and_target_controls_keep_life(self):
  for v in ('opponent_artifact_0','source_lost_artifact_1','nonartifact_death_0','hexproof_opponent_1'):
   c=self.entry_cases('Marionette Master',[v])[0];self.has(c,'assert_life',player=1-int(v[-1]),value=20)
 def test_marionette_simultaneous_wrath_uses_counter_power_last_known(self):
  c=self.entry_cases('Marionette Master',['power_wrath_snapshot_1'])[0];self.has(c,'assert_card',ref='source',property='power',value=4);self.has(c,'cast',ref='power_wrath',value=1);self.has(c,'assert_card',ref='source',property='in_play',value=0);self.has(c,'assert_life',player=0,value=16);self.has(c,'assert_recorded_state')
 def test_marionette_bounce_exile_are_not_artifact_deaths(self):
  for v in ('bounce_token_0','exile_token_1'):
   c=self.entry_cases('Marionette Master',[v])[0];self.has(c,'assert_life',player=1-int(v[-1]),value=20)
 def test_marionette_whole_followup_and_number_are_guarded(self):
  for field,value in [('fabricate',2),('followup','damage')]:
   d=self.entry_declaration('Marionette Master');d['params'][field]=value
   with self.assertRaises(ValueError):self.entry_cases('Marionette Master',d=d)
 def test_landfall_has_actual_land_entry_and_controller_negative(self):
  c=self.entry_cases('Sporemound',['create_0'])[0];self.has(c,'activate',ref='atlas',value=1);self.has(c,'choose_zone_card',player=0,value=self.rows['Forest']['csvid']);self.has(c,'assert_card',ref='atlas',property='tapped',value=1);self.has(c,'assert_zone_multiplicity',player=0,zone='battlefield',index=self.rows['Saproling']['csvid'],value=1)
  c=self.entry_cases('Sporemound',['opponent_land_1'])[0];self.has(c,'choose_zone_card',player=0,value=self.rows['Forest']['csvid']);self.has(c,'assert_zone_multiplicity',player=1,zone='battlefield',index=self.rows['Saproling']['csvid'],value=0)
 def test_landfall_source_removed_before_land_and_nonland_are_negative(self):
  c=self.entry_cases('Rampaging Baloths',['source_before_land_0'])[0];self.has(c,'destroy',ref='source');self.has(c,'assert_zone_multiplicity',player=0,zone='battlefield',index=self.rows['Beast']['csvid'],value=0)
  c=self.entry_cases('Sporemound',['other_creature_1'])[0];self.has(c,'cast',ref='other',value=1);self.assertFalse(any(s['op']=='activate' and s.get('ref')=='atlas' for s in c['steps']))
 def test_optional_landfall_accept_decline_and_replay_are_real(self):
  c=self.entry_cases('Emeria Angel',['create_0'])[0];self.has(c,'choose_trigger',player=0,ref='source')
  c=self.entry_cases('Emeria Angel',['land_decline_0'])[0];self.has(c,'activate',ref='atlas',value=1);self.assertFalse(any(s['op']=='choose_trigger' for s in c['steps']));self.has(c,'assert_zone_multiplicity',player=0,zone='battlefield',index=self.rows['Bird']['csvid'],value=0)
  c=self.entry_cases('Sporemound',['effect_snapshot_1'])[0];self.assertEqual(sum(s==dict(op='activate',ref='atlas',value=1) for s in c['steps']),2)
 def test_alternative_tapless_mode_does_not_invalidate_cardwide_probe(self):
  p=spec(self.entry_declaration('Lich Lord of Unx'),self.oracle['Lich Lord of Unx']);self.assertNotIn('tapped_denied_0',variants(p));self.assertNotIn('activation_wrong_color_0',variants(p));c=self.entry_cases('Lich Lord of Unx',['activation_payment_1'])[0];self.assertFalse(any(s['op']=='choose_dialog' for s in c['steps']))
 def test_trostani_lifelink_is_actual_life_gain_after_combat(self):
  for v in ('ready_attack_0','block_ground_1'):
   c=self.entry_cases('Trostani Discordant',[v])[0];self.has(c,'combat_damage');self.has(c,'assert_life',player=int(v[-1]),value=22)
 def test_owner_return_distinct_physical_identities_move_in_both_directions(self):
  c=self.entry_cases('Trostani Discordant',['owner_return_0'])[0];self.has(c,'cleanup');self.has(c,'bind_permanent',player=1,value=self.rows['Grizzly Bears']['csvid'],ref='unselected');self.has(c,'bind_permanent',player=0,value=self.rows['Ashcoat Bear']['csvid'],ref='lost_owned');self.has(c,'assert_zone_multiplicity',player=0,zone='battlefield',index=self.rows['Grizzly Bears']['csvid'],value=0);self.has(c,'assert_zone_multiplicity',player=0,zone='battlefield',index=self.rows['Ashcoat Bear']['csvid'],value=1);self.has(c,'assert_card',ref='lost_owned',property='power',value=3);self.has(c,'assert_card',ref='unselected',property='power',value=2)
 def test_owner_return_wrong_turn_and_missing_source_preserve_control(self):
  for v in ('owner_return_wrong_turn_1','owner_return_source_lost_1'):
   c=self.entry_cases('Trostani Discordant',[v])[0];self.has(c,'assert_zone_multiplicity',player=1,zone='battlefield',index=self.rows['Grizzly Bears']['csvid'],value=1);self.has(c,'assert_zone_multiplicity',player=0,zone='battlefield',index=self.rows['Ashcoat Bear']['csvid'],value=1)
 def test_owner_return_replay_reexecutes_real_end_step(self):
  c=self.entry_cases('Trostani Discordant',['owner_return_snapshot_1'])[0];self.assertEqual(sum(s==dict(op='cleanup') for s in c['steps']),2);self.has(c,'assert_recorded_state')
 def test_numeric_grant_scope_uses_opposing_real_creations_then_source_loss(self):
  c=self.entry_cases('Deranged Hermit',['numeric_scope_0'])[0];tid=self.rows['Squirrel']['csvid'];self.has(c,'cast',ref='scope_spell',value=1);self.has(c,'assert_matching_permanents',player=1,index=tid,property='power',value=2);self.has(c,'assert_matching_permanents',player=1,index=tid,property='power',value=1)
  c=self.entry_cases('Captain of the Watch',['numeric_scope_snapshot_1'])[0];self.has(c,'assert_matching_permanents',player=0,index=self.rows['Soldier']['csvid'],property='power',value=1);self.has(c,'assert_recorded_state')
 def test_owner_return_component_cannot_be_relabelled_inactive(self):
  d=self.entry_declaration('Trostani Discordant');d['params']['owner_return']=False
  with self.assertRaises(ValueError):self.entry_cases('Trostani Discordant',d=d)
 def test_simultaneous_artifact_witness_is_a_creature_wrath_actually_destroys(self):
  c=self.entry_cases('Marionette Master',['power_wrath_0'])[0];self.assertIn(dict(name='Cathodion',ref='artifact_witness',player=0,zone='hand'),c['setup']);self.has(c,'assert_card',ref='artifact_witness',property='power',value=3);self.has(c,'assert_mana',player=0,color=0,value=6);self.has(c,'assert_mana',player=0,color=0,value=7);self.has(c,'assert_zone_multiplicity',player=0,zone='graveyard',index=self.rows['Cathodion']['csvid'],value=1)
 def test_landfall_wrath_destroys_atlas_but_keeps_the_noncreature_forest(self):
  c=self.entry_cases('Sporemound',['wrath_0'])[0];self.has(c,'assert_card',ref='atlas',property='in_play',value=0);self.has(c,'assert_card',ref='trigger_land',property='in_play',value=1);self.has(c,'assert_zone',player=0,zone='battlefield',value=1);self.has(c,'assert_zone_multiplicity',player=0,zone='graveyard',index=self.rows['Forest']['csvid'],value=1)
 def test_baloths_keeps_mandatory_control_and_accepts_legacy_ui_for_functional_siblings(self):
  mandatory,functional=self.entry_cases('Rampaging Baloths',['create_0','ready_attack_0']);self.assertFalse(any(s['op']=='choose_trigger' for s in mandatory['steps']));self.has(mandatory,'assert_zone_multiplicity',player=0,zone='battlefield',index=self.rows['Beast']['csvid'],value=1);self.has(functional,'choose_trigger',player=0,ref='source');self.has(functional,'combat_damage');self.has(functional,'assert_life',player=1,value=16);self.assertNotIn('land_decline_0',variants(spec(self.entry_declaration('Rampaging Baloths'),self.oracle['Rampaging Baloths'])))
 def test_maverick_recast_declines_improvise_only_after_its_first_tokens_exist(self):
  for owner in (0,1):
   c=self.entry_cases('Maverick Thopterist',['bounce_recast_'+str(owner)])[0];s=c['steps'];casts=[i for i,a in enumerate(s) if a==dict(op='cast',ref='source',value=1)];self.assertEqual(len(casts),2);menus=[i for i,a in enumerate(s) if a==dict(op='choose_dialog',player=owner,value=1)];self.assertEqual(menus,[casts[1]-1]);self.assertGreater(menus[0],casts[0]);self.has(c,'assert_zone_multiplicity',player=owner,zone='battlefield',index=self.rows['Thopter']['csvid'],value=4);self.has(c,'assert_mana',player=owner,color=0,value=5)
   for index in range(4):self.has(c,'assert_card',ref='expected_token_'+str(index),property='tapped',value=0)
 def test_event_artifact_flag_rejects_integer_equal_to_the_required_boolean(self):
  for name in ('Goblin Marshal','Jund Battlemage','Oviya Pashiri, Sage Lifecrafter','Sporemound'):
   d=self.entry_declaration(name);d['params']['token']['artifact']=int(d['params']['token']['artifact'])
   with self.assertRaises(ValueError):self.entry_cases(name,d=d)
 def variable_declaration(self,name='Avenger of Zendikar'):
  from token_trigger_creation_spec import variable_data
  profile=variable_data()['profiles'][name];params={k:copy.deepcopy(profile[k]) for k in ('profile','basis','token','source_keywords','inactive_clauses','mandatory_followups')}
  return dict(family='token_trigger_creation',version=2,params=params,oracle_text=self.oracle[name]['oracle_text'],variants=['create_0'])
 def variable_cases(self,name='Avenger of Zendikar',selected=None,d=None,o=None,rows=None):
  return self.cases(name,selected,d=d or self.variable_declaration(name),o=o,rows=rows)
 def variable_final_count(self,name,variant):
  c=self.variable_cases(name,[variant])[0];tid=self.rows[self.variable_declaration(name)['params']['token']['name']]['csvid'];owner=int(variant[-1]);return [a['value'] for a in c['steps'] if a['op']=='assert_zone_multiplicity' and a['player']==owner and a['zone']=='battlefield' and a['index']==tid][-1]
 def test_variable_whole_source_and_typed_count_components(self):
  from token_trigger_creation_spec import variable_data,variable_spec
  d=self.variable_declaration();d['params']['token']['artifact']=0
  with self.assertRaises(ValueError):self.variable_cases(d=d)
  d=self.variable_declaration();d['params']['mandatory_followups'][0]['optional']=1
  with self.assertRaises(ValueError):self.variable_cases(d=d)
  r=copy.deepcopy(self.oracle['Caller of the Claw']);r['oracle_text']+='\n{T}: Draw a card.';data=variable_data();data['profiles'][r['name']]['source']=r;d=self.variable_declaration(r['name']);d['oracle_text']=r['oracle_text']
  with self.assertRaises(ValueError):variable_spec(d,r,data)
 def test_variable_creator_body_is_independent_of_profile_errors(self):
  from token_trigger_creation_spec import variable_data,variable_spec
  data=variable_data();data['profiles']['Avenger of Zendikar']['token']['power']=4;d=self.variable_declaration();d['params']['token']['power']=4
  with self.assertRaises(ValueError):variable_spec(d,self.oracle['Avenger of Zendikar'],data)
 def test_variable_count_minimum_source_inclusion_and_opponent_scope(self):
  expected={'Avenger of Zendikar':(2,0,4,0),'Evangel of Heliod':(4,2,6,2),'Caller of the Claw':(2,0,4,0),'Hallowed Spiritkeeper':(3,1,5,1),'Izoni, Thousand-Eyed':(2,0,4,0)}
  for name,values in expected.items():
   for owner in (0,1):self.assertEqual(tuple(self.variable_final_count(name,v+'_'+str(owner)) for v in ('create','minimum','larger_amount','opponent_state')),values)
 def test_variable_tokens_excluded_from_devotion_and_graveyard_cards(self):
  expected={'Avenger of Zendikar':2,'Evangel of Heliod':4,'Caller of the Claw':0,'Hallowed Spiritkeeper':1,'Izoni, Thousand-Eyed':0}
  for name,count in expected.items():self.assertEqual(self.variable_final_count(name,'tokens_do_not_contribute_0'),count)
 def test_variable_ownership_and_control_use_actual_graveyard_receipts(self):
  for name,count in [('Avenger of Zendikar',1),('Evangel of Heliod',3),('Caller of the Claw',1),('Hallowed Spiritkeeper',2),('Izoni, Thousand-Eyed',1)]:self.assertEqual(self.variable_final_count(name,'ownership_0'),count)
  c=self.variable_cases('Caller of the Claw',['ownership_1'])[0];self.has(c,'assert_zone_multiplicity',player=1,zone='graveyard',index=self.rows['Ashcoat Bear']['csvid'],value=1);self.has(c,'assert_zone_multiplicity',player=0,zone='graveyard',index=self.rows['Grizzly Bears']['csvid'],value=1)
 def test_variable_history_expiry_uses_turn_gate_and_real_flash_cast(self):
  c=self.variable_cases('Caller of the Claw',['prior_turn_expired_0'])[0];s=c['steps'];self.assertEqual(sum(a['op']=='reset_death_history' for a in s),1);death=next(i for i,a in enumerate(s) if a['op']=='destroy');turn=next(i for i,a in enumerate(s) if a['op']=='advance_turn');cast=next(i for i,a in enumerate(s) if a==dict(op='cast',ref='source',value=1));self.assertLess(death,turn);self.assertLess(turn,cast);self.assertEqual(self.variable_final_count('Caller of the Claw','prior_turn_expired_0'),0)
  self.assertEqual(self.variable_final_count('Caller of the Claw','amount_locked_0'),2)
 def test_variable_actual_purge_distinguishes_history_and_live_graveyard(self):
  self.assertEqual(self.variable_final_count('Caller of the Claw','purged_history_1'),2);self.assertEqual(self.variable_final_count('Izoni, Thousand-Eyed','purged_input_1'),0);self.assertEqual(self.variable_final_count('Hallowed Spiritkeeper','purged_input_1'),1)
  c=self.variable_cases('Caller of the Claw',['purged_history_1'])[0];self.has(c,'activate',ref='crypt',value=1);self.has(c,'choose_player',player=1,value=1)
 def test_variable_reentry_recomputes_input_and_excludes_token_feedback(self):
  for name,count in [('Avenger of Zendikar',5),('Evangel of Heliod',9),('Caller of the Claw',5),('Izoni, Thousand-Eyed',5)]:self.assertEqual(self.variable_final_count(name,'reentry_changed_input_0'),count)
  c=self.variable_cases('Avenger of Zendikar',['reentry_changed_input_0'])[0];self.assertFalse(any(a['op']=='choose_trigger' for a in c['steps']))
 def test_variable_avenger_mandatory_landfall_effect_has_scope_and_loss_controls(self):
  c=self.variable_cases('Avenger of Zendikar',['landfall_scope_0'])[0];self.has(c,'activate',ref='atlas',value=1);self.has(c,'choose_trigger',player=0,ref='source');self.has(c,'assert_card',ref='receipt_0_Plant_0',property='plus1_counters',value=1);self.has(c,'assert_card',ref='receipt_1_Plant_0',property='plus1_counters',value=0);self.has(c,'assert_card',ref='ordinary_control',property='power',value=2)
  for v in ('landfall_decline_0','landfall_source_removed_0','landfall_opponent_0'):
   c=self.variable_cases('Avenger of Zendikar',[v])[0];self.has(c,'assert_card',ref='receipt_0_Plant_0',property='plus1_counters',value=0)
 def test_variable_avenger_counters_persist_and_repeat(self):
  c=self.variable_cases('Avenger of Zendikar',['landfall_cleanup_0'])[0];self.has(c,'cleanup');self.has(c,'assert_card',ref='receipt_0_Plant_0',property='plus1_counters',value=1)
  c=self.variable_cases('Avenger of Zendikar',['landfall_repeat_0'])[0];self.has(c,'assert_card',ref='receipt_0_Plant_0',property='plus1_counters',value=2)
 def test_variable_death_counts_simultaneous_other_creature_and_excludes_nondeath(self):
  self.assertEqual(self.variable_final_count('Hallowed Spiritkeeper','wrath_death_0'),4)
  for v in ('countered_0','bounce_source_0','exile_source_0'):self.assertEqual(self.variable_final_count('Hallowed Spiritkeeper',v),0)
 def test_variable_snapshots_replay_real_sources_effects_and_controllers(self):
  for name,v in [('Caller of the Claw','ready_snapshot_1'),('Hallowed Spiritkeeper','pending_snapshot_1'),('Izoni, Thousand-Eyed','event_snapshot_1'),('Avenger of Zendikar','landfall_scope_snapshot_1')]:
   c=self.variable_cases(name,[v])[0];s=c['steps'];restore=s.index(dict(op='restore'));self.has(c,'assert_recorded_state');self.assertTrue(any(a['op'] in ('cast','activate','destroy','resolve') for a in s[restore:]));self.assertTrue(any(a['op']=='assert_zone_multiplicity' for a in s[restore:]))
 def test_variable_real_payment_and_printed_flying_combat(self):
  from token_trigger_creation_spec import variable_data
  for name in variable_data()['profiles']:
   c=self.variable_cases(name,['payment_1'])[0];self.has(c,'cast',ref='source',value=1)
   for color in range(6):self.has(c,'assert_mana',player=1,color=color,value=0)
  c=self.variable_cases('Hallowed Spiritkeeper',['block_flying_0'])[0];self.has(c,'block',ref='test_token',other='combat_witness',value=1)
  c=self.variable_cases('Caller of the Claw',['block_flying_0'])[0];self.has(c,'block',ref='test_token',other='combat_witness',value=0)
 def test_variable_expected_rules_do_not_use_token_catalog_or_handlers(self):
  o=copy.deepcopy(self.oracle);rows=copy.deepcopy(self.rows)
  for token in ('Plant','Bear','Soldier','Spirit','Insect'):o[token].update(power='99',toughness='99',colors=['R'],oracle_text='Deathtouch')
  for row in rows.values():row['handler']='card_dummy'
  from token_trigger_creation_spec import variable_data
  for name in variable_data()['profiles']:self.assertEqual(self.variable_cases(name),self.variable_cases(name,o=o,rows=rows))
 def test_all_variable_profiles_expand_selected_catalog(self):
  from token_trigger_creation_spec import variable_data
  count=0
  for name in variable_data()['profiles']:
   d=self.variable_declaration(name);p=spec(d,self.oracle[name]);count+=len(self.variable_cases(name,variants(p)))
  self.assertEqual(count,267)
 def test_variable_nonlive_zone_offsets_are_never_inspected_as_instance_handles(self):
  for name in ('Avenger of Zendikar','Evangel of Heliod','Caller of the Claw','Hallowed Spiritkeeper','Izoni, Thousand-Eyed'):
   c=self.variable_cases(name)[0];nonlive={item['ref'] for item in c['setup'] if item['zone'] in ('library','graveyard','exile')};self.assertTrue(nonlive);self.assertFalse(any(a['op']=='assert_card' and a.get('ref') in nonlive for a in c['steps']));self.assertTrue(any(a['op']=='assert_zone_card' and a['zone']=='library' for a in c['steps']));self.assertTrue(any(a['op']=='assert_zone_multiplicity' and a['zone']=='graveyard' for a in c['steps']));self.has(c,'assert_card',ref='source',property='in_play',value=int(name!='Hallowed Spiritkeeper'))
 def test_whole_pin_and_parameter_schema(self):
  for d in (dict(self.declaration(),oracle_text='changed'),dict(self.declaration(),params={}),dict(self.declaration(),params=dict(self.declaration()['params'],optional=True))):
   with self.assertRaises(ValueError):self.cases(d=d)
 def test_token_characteristics_and_boolean_types_pin_creator_text(self):
  for changes in ({'count':2},{'count':True},{'power':2},{'toughness':2},{'colors':['B']},{'keywords':[]},{'artifact':1},{'subtypes':['Soldier']},{'name':'Soldier'}):
   d=self.declaration();d['params']['token'].update(changes)
   with self.assertRaises(ValueError):self.cases(d=d)
 def test_unreviewed_additional_text_and_source_forms_reject(self):
  d=self.declaration();r=self.oracle['Doomed Traveler']
  for change in (dict(oracle_text=r['oracle_text']+'\n{T}: Draw a card.'),dict(layout='transform'),dict(selected_face='front'),dict(card_faces=[{}]),dict(type_line='Token Creature — Human')):
   x=dict(r,**change);d['oracle_text']=x['oracle_text']
   with self.assertRaises(ValueError):spec(d,x)
 def test_cast_only_does_not_create_death_tokens(self):
  for c in self.cases(selected=['cast_only_0','cast_only_1']):
   owner=int(c['_recipe']['variant'][-1]);self.has(c,'cast',ref='source',value=1);self.has(c,'assert_zone_multiplicity',player=owner,zone='battlefield',index=self.rows['Spirit']['csvid'],value=0)
 def test_death_follows_real_cast_and_changes_source_zone(self):
  for c in self.cases(selected=['create_0','create_1']):
   owner=int(c['_recipe']['variant'][-1]);self.has(c,'destroy',ref='source');self.has(c,'assert_card',ref='source',property='in_play',value=0);self.has(c,'assert_zone_multiplicity',player=owner,zone='graveyard',index=self.rows['Doomed Traveler']['csvid'],value=1);self.has(c,'assert_zone_multiplicity',player=owner,zone='battlefield',index=self.rows['Spirit']['csvid'],value=1)
 def test_bounce_blink_and_exile_are_not_death(self):
  for c in self.cases(selected=['bounce_only_0','blink_only_1','exile_source_0']):
   owner=int(c['_recipe']['variant'][-1]);self.has(c,'assert_zone_multiplicity',player=owner,zone='battlefield',index=self.rows['Spirit']['csvid'],value=0)
 def test_reentry_then_death_only_creates_once(self):
  for c in self.cases(selected=['bounce_recast_0','blink_destroy_1']):
   owner=int(c['_recipe']['variant'][-1]);self.has(c,'assert_zone_multiplicity',player=owner,zone='battlefield',index=self.rows['Spirit']['csvid'],value=1)
 def test_other_creature_death_does_not_create(self):
  c=self.cases(selected=['other_creature_1'])[0];self.has(c,'destroy',ref='other');self.has(c,'assert_zone_multiplicity',player=1,zone='battlefield',index=self.rows['Spirit']['csvid'],value=0)
 def test_lethal_spell_and_combat_are_real_death_paths(self):
  for v in ('murder_0','lethal_damage_1','combat_death_0'):
   c=self.cases(selected=[v])[0];self.has(c,'assert_zone_multiplicity',player=int(v[-1]),zone='battlefield',index=self.rows['Spirit']['csvid'],value=1)
   self.assertTrue(any(s['op'] in ('cast','combat_damage') for s in c['steps']))
 def test_token_catalog_characteristics_never_supply_expectations(self):
  o=copy.deepcopy(self.oracle);o['Spirit'].update(power='99',toughness='99',oracle_text='Trample',colors=['R'])
  self.assertEqual(self.cases(),self.cases(o=o))
 def test_handler_names_are_identity_only(self):
  rows=copy.deepcopy(self.rows)
  for r in rows.values():r['handler']='card_dummy'
  self.assertEqual(self.cases(),self.cases(rows=rows))
 def test_full_witness_text_cost_and_characteristics_are_guarded(self):
  for nm,k,value in (('Murder','oracle_text','changed'),('Scaled Wurm','power','8'),('Unsummon','mana_cost','{2}{U}'),('Grizzly Bears','colors',['R'])):
   o=copy.deepcopy(self.oracle);o[nm][k]=value
   with self.assertRaises(ValueError):self.cases(o=o)
 def test_token_lifelink_changes_life_in_actual_combat(self):
  c=self.cases('Hunted Witness',['ready_attack_0'])[0];self.has(c,'combat_damage');self.has(c,'assert_life',player=0,value=21);self.has(c,'assert_life',player=1,value=19)
 def test_token_reach_and_flying_have_legal_blocker_controls(self):
  c=self.cases('Penumbra Spider',['block_flying_0'])[0];self.has(c,'block',ref='token',other='witness',value=1)
 def test_token_leaving_never_inserts_it_in_hand_grave_or_exile(self):
  for c in self.cases(selected=['destroy_token_0','bounce_token_1','exile_token_0']):
   owner=int(c['_recipe']['variant'][-1])
   for zone in ('hand','graveyard','exile'):self.has(c,'assert_zone_multiplicity',player=owner,zone=zone,index=self.rows['Spirit']['csvid'],value=0)
 def test_activation_payment_is_separate_from_source_cast(self):
  c=self.cases('Jade Mage',['activation_payment_1'])[0];self.has(c,'cast',ref='source',value=1);self.has(c,'activate',ref='source',value=1);self.has(c,'assert_stack',value=1)
  for k in range(6):self.has(c,'assert_mana',player=1,color=k,value=0)
 def test_activation_insufficient_and_wrong_color_are_negative_controls(self):
  for c in self.cases('Ant Queen',['activation_insufficient_0','activation_wrong_color_1']):self.has(c,'activate',ref='source',value=0)
 def test_self_sacrifice_is_a_cost_before_resolution(self):
  c=self.cases("Centaur's Herald",['create_0'])[0];self.has(c,'activate',ref='source',value=1);self.has(c,'assert_card',ref='source',property='in_play',value=0);self.has(c,'assert_zone_multiplicity',player=0,zone='graveyard',index=self.rows["Centaur's Herald"]['csvid'],value=1)
 def test_spell_event_uses_real_witness_and_other_caster_for_removal(self):
  for name in ('Young Pyromancer','Dragonlair Spider'):
   c=self.cases(name,['create_1'])[0];self.has(c,'cast',ref='trigger_spell',value=1);self.has(c,'choose_target',player=1 if name=='Young Pyromancer' else 0,ref='unselected')
 def test_afterlife_and_inactive_components_are_explicit(self):
  c=self.cases('Seraph of the Scales')[0];self.assertTrue(any('inactive: {W}:' in s for s in c['_recipe']['untested_components']));self.has(c,'assert_zone_multiplicity',player=0,zone='battlefield',index=self.rows['Spirit']['csvid'],value=2)
 def test_snapshots_replay_event_and_positive_token_checks(self):
  for c in self.cases(selected=['ready_snapshot_0','pending_snapshot_1','effect_snapshot_0','resolved_snapshot_1']):
   self.has(c,'ai_roundtrip');self.has(c,'assert_recorded_state');tail=c['steps'][c['steps'].index(dict(op='restore'))+1:];self.assertTrue(any(s['op']=='assert_matching_permanents' for s in tail))
 def test_roc_legacy_identity_preserves_printed_bird_properties(self):
  c=self.cases('Roc Egg')[0];self.has(c,'assert_zone_multiplicity',player=0,zone='battlefield',index=self.rows['Roc']['csvid'],value=1);self.has(c,'assert_matching_permanents',player=0,index=self.rows['Roc']['csvid'],property='subtype_Bird',value=1);self.has(c,'assert_matching_permanents',player=0,index=self.rows['Roc']['csvid'],property='power',value=3)
 def test_attack_trigger_completes_legal_empty_combat(self):
  c=self.cases('Grave Titan')[0];steps=c['steps'];self.assertLess(steps.index(dict(op='combat_damage')),steps.index(dict(op='end_combat')))
 def test_dash_normal_cast_menu_only_when_alternative_affordable(self):
  c=self.cases('Mardu Strike Leader')[0];self.has(c,'choose_dialog',player=0,value=1)
  c=self.cases('Mardu Strike Leader',['payment_0'])[0];self.assertFalse(any(s['op']=='choose_dialog' for s in c['steps']))
 def test_turn_gate_omits_untap_and_preserves_tapped_source(self):
  for name in ('Grave Titan','Thraben Doomsayer'):
   c=self.cases(name,['ready_attack_0'])[0];self.has(c,'assert_card',ref='source',property='tapped',value=1)
 def test_jade_mage_printed_cost_is_not_changed_to_legacy_bug(self):
  p=spec(self.declaration('Jade Mage'),self.oracle['Jade Mage']);self.assertEqual(p['activation']['mana'],[2,0,0,1,0,0])
 def test_fresh_attack_selects_new_attack_token_after_older_etb_tokens(self):
  c=self.cases('Grave Titan',['fresh_attack_0'])[0];self.has(c,'bind_permanent_copy',player=0,value=self.rows['Zombie']['csvid'],index=3,ref='token');self.has(c,'attack',ref='token',value=0)
 def test_normal_cast_choice_precedes_counterspell_priority_input(self):
  c=self.cases('Mardu Strike Leader',['countered_0'])[0];s=c['steps'];self.assertLess(s.index(dict(op='choose_dialog',player=0,value=1)),s.index(dict(op='choose_priority',ref='counter')))
 def test_twiddle_tap_choice_is_scripted(self):
  c=self.cases('Thraben Doomsayer',['tapped_denied_0'])[0];self.has(c,'choose_dialog',player=0,value=0)
 def test_zero_mana_tap_creation_checks_real_cost_and_gap(self):
  c=self.cases('Thraben Doomsayer',['create_0'])[0];self.has(c,'activate',ref='source',value=1);self.has(c,'assert_card',ref='source',property='tapped',value=1);self.has(c,'assert_zone_multiplicity',player=0,zone='battlefield',index=self.rows['Human']['csvid'],value=1);self.assertTrue(any('Fateful hour' in s for s in c['_recipe']['untested_components']))
  self.assertNotIn('activation_insufficient_0',variants(spec(self.declaration('Thraben Doomsayer'),self.oracle['Thraben Doomsayer'])))
 def test_positive_receipt_precedes_cleanup(self):
  c=self.cases(selected=['cleanup_0'])[0];actions=c['steps'];cut=actions.index(dict(op='cleanup'));self.assertTrue(any(a['op']=='assert_matching_permanents' and a.get('property')=='power' for a in actions[:cut]))
 def test_typed_complete_activation_cost_schema(self):
  for activation in (None,{},dict(mana=[0,0,0,True,0,0],tap=False,sacrifice=False),dict(mana=[0,0,0,1,0,0],tap=0,sacrifice=False)):
   d=self.declaration('Jade Mage');d['params']['activation']=activation
   with self.assertRaises(ValueError):self.cases('Jade Mage',d=d)
 def test_large_fixed_death_token_count_and_both_colors(self):
  c=self.cases('Symbiotic Wurm')[0];self.has(c,'bind_permanent_copy',player=0,value=self.rows['Insect']['csvid'],index=6,ref='expected_token_6')
  c=self.cases('Imperious Oligarch')[0];self.has(c,'assert_matching_permanents',player=0,index=self.rows['Spirit']['csvid'],property='color',value=34)

class DeathWatchComponentTest(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.oracle=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards'];cls.rows={r['name']:r for r in registry()}
 def declaration(self,name):
  from token_trigger_creation_spec import death_watch_data
  p=death_watch_data()[name];return dict(family='token_trigger_creation' if 'Creature' in self.oracle[name]['type_line'] else 'token_spell_extension',version=2,params={k:copy.deepcopy(p[k]) for k in ('profile','token','predicate','optional','token_mana','source_keywords')},oracle_text=self.oracle[name]['oracle_text'],variants=['create_0'])
 def cases(self,name,variants=None,d=None,o=None,rows=None):
  from recipes import expand as public_expand
  d=d or self.declaration(name)
  if variants:d['variants']=variants
  return public_expand(name,self.rows[name]['csvid'],d,(o or self.oracle)[name],rows or self.rows,oracle_context=o or self.oracle)
 def count(self,name,variant):
  c=self.cases(name,[variant])[0];tid=self.rows[self.declaration(name)['params']['token']['name']]['csvid'];return [a['value'] for a in c['steps'] if a['op']=='assert_zone_multiplicity' and a['zone']=='battlefield' and a['player']==int(variant[-1]) and a['index']==tid][-1]
 def has(self,c,op,**fields):self.assertIn(dict(op=op,**fields),c['steps'])
 def test_full_source_and_boolean_schema_are_guarded(self):
  for name in ('Pawn of Ulamog','Field of Souls','Prowess of the Fair'):
   for field in ('optional','token_mana'):
    d=self.declaration(name);d['params'][field]=int(d['params'][field])
    with self.assertRaises(ValueError):self.cases(name,d=d)
   d=self.declaration(name);d['params']['token']['artifact']=0
   with self.assertRaises(ValueError):self.cases(name,d=d)
 def test_same_line_extra_mandatory_effect_is_rejected_independently(self):
  from token_trigger_creation_spec import death_watch_data,death_watch_spec
  name='Field of Souls';data=death_watch_data();r=copy.deepcopy(self.oracle[name]);r['oracle_text']+=' Draw a card.';data[name]['source']=r;d=self.declaration(name);d['oracle_text']=r['oracle_text']
  with self.assertRaises(ValueError):death_watch_spec(d,r,data)
 def test_token_named_oracle_collision_cannot_be_a_normal_physical_witness(self):
  from token_trigger_creation_spec import variable_data,validate_variable_witnesses,death_watch_spec
  data=variable_data();data['cards']['Llanowar Elves']=copy.deepcopy(self.oracle['Llanowar Elves']);p=death_watch_spec(self.declaration('Prowess of the Fair'),self.oracle['Prowess of the Fair'])
  with self.assertRaisesRegex(ValueError,'normal physical witness'):validate_variable_witnesses(self.oracle,self.rows,p,data)
  self.assertEqual(data['cards']['Elvish Mystic']['layout'],'normal');self.assertEqual(data['cards']['Elvish Mystic']['mana_cost'],'{G}')
 def test_distinct_owner_controller_and_global_predicates(self):
  expected={'Field of Souls':(0,1,0),'Golgari Germination':(1,0,0),'Open the Graves':(1,0,0),'Requiem Angel':(1,0,0),'Pawn of Ulamog':(1,0,0),'Sifter of Skulls':(1,0,0),'Rotlung Reanimator':(1,1,1),'Prowess of the Fair':(0,1,0)}
  for name,values in expected.items():
   for owner in (0,1):self.assertEqual(tuple(self.count(name,v+'_'+str(owner)) for v in ('foreign_controlled','owned_opposing','opponent_death')),values)
 def test_real_foreign_death_preserves_actual_owner_grave_receipt(self):
  c=self.cases('Field of Souls',['foreign_controlled_0'])[0];self.has(c,'destroy',ref='victim');self.has(c,'assert_zone_multiplicity',player=1,zone='graveyard',index=self.rows['Grizzly Bears']['csvid'],value=1);self.assertEqual(self.count('Field of Souls','foreign_controlled_0'),0)
 def test_wrath_uses_same_event_lki_but_retains_noncreature_sources(self):
  for name,n in [('Field of Souls',2),('Golgari Germination',2),('Open the Graves',2),('Requiem Angel',2),('Pawn of Ulamog',3),('Sifter of Skulls',2),('Rotlung Reanimator',4),('Prowess of the Fair',2)]:
   c=self.cases(name,['wrath_death_0'])[0];self.assertEqual(self.count(name,'wrath_death_0'),n);self.has(c,'cast',ref='wrath',value=1);self.has(c,'assert_card',ref='source',property='in_play',value=int('Creature' not in self.oracle[name]['type_line']))
 def test_own_source_death_inclusion_and_other_exclusion(self):
  for name,n in [('Requiem Angel',0),('Pawn of Ulamog',1),('Sifter of Skulls',0),('Rotlung Reanimator',1)]:self.assertEqual(self.count(name,'source_death_1'),n)
 def test_real_nondeath_paths_do_not_trigger(self):
  from token_trigger_creation_spec import death_watch_data
  for name in death_watch_data():
   for v in ('cast_only','prior_death','bounce_only','exile_only','noncreature_death'):self.assertEqual(self.count(name,v+'_0'),0)
 def test_requiem_includes_independent_token_death_and_excludes_spirit_token(self):
  self.assertEqual(self.count('Requiem Angel','independent_token_death_1'),1);self.assertEqual(self.count('Requiem Angel','spirit_token_negative_0'),0);c=self.cases('Requiem Angel',['independent_token_death_1'])[0];self.has(c,'cast',ref='independent_token_spell',value=1);self.has(c,'destroy',ref='independent_token')
 def test_prowess_accepts_actual_noncreature_kindred_elf_death(self):
  c=self.cases('Prowess of the Fair',['kindred_elf_death_0'])[0];self.has(c,'cast',ref='kindred_elf',value=1);self.has(c,'destroy',ref='kindred_elf');self.has(c,'assert_card',ref='source',property='in_play',value=1);self.assertEqual(self.count('Prowess of the Fair','kindred_elf_death_0'),1)
 def test_optional_human_choices_and_decline_remain_distinct(self):
  for name in ('Pawn of Ulamog','Prowess of the Fair'):
   c=self.cases(name)[0];self.has(c,'choose_priority',ref='source');c=self.cases(name,['decline_0'])[0];self.assertFalse(any(a['op']=='choose_trigger' for a in c['steps']));self.assertEqual(self.count(name,'decline_0'),0)
  c=self.cases('Sifter of Skulls')[0];self.assertFalse(any(a['op']=='choose_trigger' for a in c['steps']))
 def test_sifter_devoid_and_requiem_flying_are_operative(self):
  c=self.cases('Sifter of Skulls')[0];self.has(c,'assert_matching_permanents',player=0,index=self.rows['Sifter of Skulls']['csvid'],property='color',value=0);c=self.cases('Requiem Angel',['source_keyword_combat_0'])[0];self.has(c,'attack',ref='source',value=1);self.has(c,'block',ref='ground_blocker',other='source',value=0);self.has(c,'assert_life',player=1,value=15)
 def test_full_death_watch_observations_use_supported_protocol(self):
  from generate_cases import PROPERTIES,TOKEN_PROPERTIES,OPS
  from token_trigger_creation_spec import death_watch_data,death_watch_spec,death_watch_variants
  for name in death_watch_data():
   p=death_watch_spec(self.declaration(name),self.oracle[name])
   for c in self.cases(name,death_watch_variants(p)):
    for a in c['steps']:
     self.assertIn(a['op'],OPS);self.assertIn(a.get('property','power'),PROPERTIES|(TOKEN_PROPERTIES if a['op']=='assert_matching_permanents' else set()))
 def test_actual_spawn_scion_sacrifice_mana_is_not_a_token_death_loop(self):
  from token_trigger_creation_spec import variable_cost
  for name in ('Pawn of Ulamog','Sifter of Skulls'):
   c=self.cases(name,['token_mana_1'])[0];self.has(c,'activate',ref='mana_token',value=1);self.has(c,'assert_stack',value=0);self.has(c,'assert_card',ref='mana_token',property='in_play',value=0);self.assertEqual(self.count(name,'token_mana_1'),0)
   for color,paid in enumerate(variable_cost(self.oracle[name])):self.has(c,'assert_mana',player=1,color=color,value=(31 if color==0 else 10)-paid)
 def test_source_loss_is_after_positive_effect_and_blocks_later_death(self):
  for name,n in [('Field of Souls',1),('Golgari Germination',1),('Open the Graves',1),('Requiem Angel',1),('Pawn of Ulamog',2),('Sifter of Skulls',1),('Rotlung Reanimator',2),('Prowess of the Fair',1)]:
   c=self.cases(name,['source_removed_0'])[0];self.assertEqual(self.count(name,'source_removed_0'),n);s=c['steps'];death=s.index(dict(op='destroy',ref='source'));self.assertTrue(any(a['op']=='assert_matching_permanents' for a in s[:death]));self.assertLess(death,s.index(dict(op='destroy',ref='second_victim')))
 def test_optional_real_source_binding_is_distinct_from_legacy_on_source_death(self):
  c=self.cases('Pawn of Ulamog')[0];self.has(c,'choose_priority',ref='source');self.assertFalse(any(a['op']=='choose_trigger' for a in c['steps']));c=self.cases('Pawn of Ulamog',['source_death_0'])[0];self.has(c,'choose_trigger',player=0,ref='source')
 def test_both_prewritten_copies_count_in_first_pending_physical_hand(self):
  c=self.cases('Prowess of the Fair',['kindred_elf_death_0'])[0];tid=self.rows['Prowess of the Fair']['csvid'];self.has(c,'assert_zone_multiplicity',player=0,zone='hand',index=tid,value=2);self.has(c,'assert_matching_permanents',player=0,index=tid,property='subtype_Elf',value=1)
 def test_murder_target_input_precedes_its_optional_death_trigger_input(self):
  for name in ('Pawn of Ulamog','Prowess of the Fair'):
   c=self.cases(name,['murder_0'])[0];s=c['steps'];target=s.index(dict(op='choose_target',player=0,ref='victim'));trigger=s.index(dict(op='choose_priority',ref='source'));cast=s.index(dict(op='cast',ref='lethal_spell',value=1));self.assertLess(target,trigger);self.assertLess(trigger,cast)
 def test_post_source_ghost_dialog_is_expected_unused_and_preserves_no_new_token(self):
  c=self.cases('Prowess of the Fair',['source_removed_1'])[0];s=c['steps'];d=s.index(dict(op='destroy',ref='second_victim'));self.assertEqual(s[d-1],dict(op='choose_dialog',player=1,value=0));self.has(c,'assert_unused_choices',value=1);self.assertEqual(self.count('Prowess of the Fair','source_removed_1'),1)
 def test_witnesses_and_creator_rules_ignore_catalog_bodies_and_handler_names(self):
  from token_trigger_creation_spec import death_watch_data
  o=copy.deepcopy(self.oracle);rows=copy.deepcopy(self.rows)
  for name in ('Spirit','Saproling','Zombie','Eldrazi Spawn','Eldrazi Scion','Elf Warrior'):o[name].update(power='99',toughness='99',colors=['R'],oracle_text='Trample')
  for row in rows.values():row['handler']='card_dummy'
  for name in death_watch_data():self.assertEqual(self.cases(name),self.cases(name,o=o,rows=rows))
  o=copy.deepcopy(self.oracle);o['Elvish Mystic']['oracle_text']=''
  with self.assertRaises(ValueError):self.cases('Prowess of the Fair',o=o)
 def test_snapshots_replay_actual_deaths_and_mana_with_receipts(self):
  for name,v in [('Requiem Angel','wrath_snapshot_1'),('Pawn of Ulamog','token_mana_snapshot_0'),('Prowess of the Fair','pending_snapshot_1'),('Field of Souls','event_snapshot_0')]:
   c=self.cases(name,[v])[0];self.has(c,'assert_recorded_state');tail=c['steps'][c['steps'].index(dict(op='restore'))+1:];self.assertTrue(any(a['op'] in ('destroy','cast','activate','resolve') for a in tail));self.assertTrue(any(a['op']=='assert_zone_multiplicity' for a in tail))
 def test_all_eight_death_watch_profiles_expand_unique_full_catalog(self):
  from token_trigger_creation_spec import death_watch_data,death_watch_spec,death_watch_variants
  total=0
  for name in death_watch_data():
   p=death_watch_spec(self.declaration(name),self.oracle[name]);vv=death_watch_variants(p);self.assertEqual(len(vv),len(set(vv)));total+=len(self.cases(name,vv))
  self.assertEqual(total,528)

class InspiredTokenComponentTest(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.oracle=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards'];cls.rows={r['name']:r for r in registry()}
 def declaration(self,name='Aerie Worshippers'):
  from token_trigger_creation_spec import inspired_data
  p=inspired_data()[name];return dict(family='token_trigger_creation',version=2,params={k:copy.deepcopy(p[k]) for k in ('profile','token','count','payment','source_keywords')},oracle_text=self.oracle[name]['oracle_text'],variants=['create_0'])
 def cases(self,name='Aerie Worshippers',variants=None,d=None,o=None,rows=None):
  from recipes import expand as public_expand
  d=d or self.declaration(name)
  if variants:d['variants']=variants
  return public_expand(name,self.rows[name]['csvid'],d,(o or self.oracle)[name],rows or self.rows,oracle_context=o or self.oracle)
 def count(self,name,variant):
  c=self.cases(name,[variant])[0];tid=self.rows[self.declaration(name)['params']['token']['name']]['csvid'];return [a['value'] for a in c['steps'] if a['op']=='assert_zone_multiplicity' and a['zone']=='battlefield' and a['player']==int(variant[-1]) and a['index']==tid][-1]
 def has(self,c,op,**fields):self.assertIn(dict(op=op,**fields),c['steps'])
 def test_typed_complete_source_token_and_payment_grammar(self):
  for field,value in [('count',True),('payment',[2,0,True,0,0,0]),('source_keywords',['Haste'])]:
   d=self.declaration();d['params'][field]=value
   with self.assertRaises(ValueError):self.cases(d=d)
  for field,value in [('enchantment',1),('artifact',0),('power',3),('colors',['R'])]:
   d=self.declaration();d['params']['token'][field]=value
   with self.assertRaises(ValueError):self.cases(d=d)
 def test_extra_same_line_mandatory_effect_cannot_hide_in_profile_pin(self):
  from token_trigger_creation_spec import inspired_data,inspired_spec
  data=inspired_data();r=copy.deepcopy(self.oracle['Aerie Worshippers']);r['oracle_text']+=' Draw a card.';data[r['name']]['source']=r;d=self.declaration();d['oracle_text']=r['oracle_text']
  with self.assertRaises(ValueError):inspired_spec(d,r,data)
 def test_real_mainphase_twiddle_taps_then_untaps_and_creates_printed_amount(self):
  from token_trigger_creation_spec import inspired_data
  for name,p in inspired_data().items():
   for owner in (0,1):
    c=self.cases(name,['create_'+str(owner)])[0];self.has(c,'choose_dialog',player=0,value=0);self.has(c,'choose_dialog',player=0,value=1);self.has(c,'cast',ref='twiddle_0',value=1);self.has(c,'cast',ref='twiddle_1',value=1);self.assertEqual(self.count(name,'create_'+str(owner)),p['count']);self.assertFalse(any(a['op']=='untap_step' for a in c['steps']))
 def test_exact_trigger_payment_is_separate_from_source_and_twiddle_cost(self):
  from token_trigger_creation_spec import inspired_data
  for name in inspired_data():
   for owner in (0,1):
    c=self.cases(name,['payment_'+str(owner)])[0]
    for color in range(6):self.has(c,'assert_mana',player=owner,color=color,value=0)
    self.assertEqual(self.count(name,'payment_'+str(owner)),self.declaration(name)['params']['count'])
 def test_unaffordable_or_wrong_color_payment_does_not_generate(self):
  from token_trigger_creation_spec import inspired_data
  for name in inspired_data():
   for owner in (0,1):
    for variant in ('payment_insufficient','payment_wrong_color'):self.assertEqual(self.count(name,variant+'_'+str(owner)),0)
 def test_decline_is_a_real_legal_generate_menu_cancel(self):
  from token_trigger_creation_spec import inspired_data
  for name in inspired_data():
   for owner in (0,1):
    c=self.cases(name,['decline_'+str(owner)])[0];self.has(c,'choose_dialog',player=owner,value=1);self.assertEqual(self.count(name,'decline_'+str(owner)),0)
 def test_untap_step_has_no_payment_tokens_or_fake_upkeep_resolution(self):
  from token_trigger_creation_spec import inspired_data
  for name in inspired_data():
   for owner in (0,1):
    c=self.cases(name,['untap_timing_'+str(owner)])[0];s=c['steps'];u=s.index(dict(op='untap_step',player=owner));self.assertEqual(self.count(name,'untap_timing_'+str(owner)),0);self.assertFalse(any(a['op'] in ('resolve','cast','activate','turn') for a in s[u+1:]));self.has(c,'assert_stack',value=0)
    for i in range(3):self.has(c,'assert_card',ref='payment_land_'+str(i),property='tapped',value=0)
    for player in (0,1):
     for color in range(6):self.has(c,'assert_mana',player=player,color=color,value=0)
    self.has(c,'assert_unused_choices',value=1);self.has(c,'choose_dialog',player=owner,value=1)
    self.assertTrue(any('following real upkeep' in g for g in c['_recipe']['untested_components']))
 def test_untap_wrapper_uses_prepared_turn_and_no_intervening_turn_setter(self):
  for owner in (0,1):
   c=self.cases(variants=['untap_timing_'+str(owner)])[0];s=c['steps'];u=s.index(dict(op='untap_step',player=owner));a=u-2;self.assertEqual(s[u-1],dict(op='choose_dialog',player=owner,value=1));self.assertEqual(s[a],dict(op='advance_turn'));self.assertEqual(s[a-1],dict(op='advance_turn'));self.assertEqual(s[a-13:a-1],[dict(op='set_mana',player=p,color=color,value=0) for p in (0,1) for color in range(6)])
 def test_opponent_step_and_unrelated_or_already_untapped_creature_are_negative(self):
  from token_trigger_creation_spec import inspired_data
  for name in inspired_data():
   for v in ('already_untapped','other_untaps','opponent_untap','source_destroyed','source_bounced','source_exiled'):self.assertEqual(self.count(name,v+'_0'),0)
 def test_enchantment_type_has_actual_card_specific_target_before_after_and_destruction(self):
  for name in ('Aerie Worshippers','God-Favored General'):
   c=self.cases(name,['enchantment_type_1'])[0];self.has(c,'require_can_cast',ref='type_witness',value=0);self.has(c,'require_can_cast',ref='type_witness',value=1);self.has(c,'choose_target',player=1,ref='test_token');self.has(c,'cast',ref='type_witness',value=1);self.has(c,'assert_matching_permanents',player=1,index=self.rows[self.declaration(name)['params']['token']['name']]['csvid'],property='artifact',value=0);self.assertEqual(self.count(name,'enchantment_type_1'),self.declaration(name)['params']['count']-1)
 def test_token_and_source_haste_are_actual_fresh_attacks(self):
  c=self.cases('Satyr Nyx-Smith',['fresh_attack_0'])[0];self.has(c,'attack',ref='test_token',value=1);self.has(c,'assert_life',player=1,value=17);self.assertFalse(any(a['op']=='advance_turn' for a in c['steps']));c=self.cases('Aerie Worshippers',['fresh_attack_0'])[0];self.has(c,'attack',ref='test_token',value=0)
  c=self.cases('Satyr Nyx-Smith',['source_haste_1'])[0];self.has(c,'attack',ref='source',value=1);self.has(c,'assert_life',player=0,value=18);self.assertEqual(self.count('Satyr Nyx-Smith','source_haste_1'),0)
 def test_source_intimidate_has_ground_negative_and_artifact_samecolor_positives(self):
  for v,legal in [('source_intimidate_ground',0),('source_intimidate_artifact',1),('source_intimidate_black',1)]:
   c=self.cases('Forlorn Pseudamma',[v+'_0'])[0];self.has(c,'block',ref='source_blocker',other='source',value=legal);self.has(c,'combat_damage')
 def test_flying_tokens_evade_ground_blockers_and_block_flying_attackers(self):
  c=self.cases('Aerie Worshippers',['flying_evasion_1'])[0];self.has(c,'block',ref='ground_blocker',other='test_token',value=0);c=self.cases('Aerie Worshippers',['block_flying_0'])[0];self.has(c,'block',ref='test_token',other='combat_witness',value=1)
 def test_printed_token_rules_ignore_registry_traits_and_all_handlers(self):
  from token_trigger_creation_spec import inspired_data
  o=copy.deepcopy(self.oracle);rows=copy.deepcopy(self.rows)
  for name in ('Bird','Zombie','Soldier','Centaur','Elemental'):o[name].update(power='99',toughness='99',colors=['W'],oracle_text='Deathtouch')
  for row in rows.values():row['handler']='card_dummy'
  for name in inspired_data():self.assertEqual(self.cases(name),self.cases(name,o=o,rows=rows))
 def test_source_witness_full_pins_are_normal_and_guarded(self):
  o=copy.deepcopy(self.oracle);o['Twiddle']['oracle_text']='Tap target creature.'
  with self.assertRaises(ValueError):self.cases(o=o)
  o=copy.deepcopy(self.oracle);o['Disenchant']['mana_cost']='{W}'
  with self.assertRaises(ValueError):self.cases(o=o)
 def test_snapshots_replay_actual_mainphase_untap_and_payment(self):
  for v in ('ready_snapshot_0','pending_snapshot_1','effect_snapshot_0','repeat_snapshot_1'):
   c=self.cases(variants=[v])[0];self.has(c,'assert_recorded_state');tail=c['steps'][c['steps'].index(dict(op='restore'))+1:];self.assertTrue(any(a['op'] in ('cast','resolve') for a in tail));self.assertTrue(any(a['op']=='assert_zone_multiplicity' for a in tail))
 def test_all_five_profiles_expand_complete_supported_protocol(self):
  from token_trigger_creation_spec import inspired_data,inspired_spec,inspired_variants
  from generate_cases import OPS,PROPERTIES,TOKEN_PROPERTIES
  total=0
  for name in inspired_data():
   p=inspired_spec(self.declaration(name),self.oracle[name]);vv=inspired_variants(p);self.assertEqual(len(vv),len(set(vv)));cases=self.cases(name,vv);total+=len(cases)
   for c in cases:
    for a in c['steps']:self.assertIn(a['op'],OPS);self.assertIn(a.get('property','power'),PROPERTIES|(TOKEN_PROPERTIES if a['op']=='assert_matching_permanents' else set()))
  self.assertEqual(total,320)
 def test_visible_ai_generate_input_is_bound_and_type_probe_restores_real_priority(self):
  c=self.cases(variants=['create_1'])[0];self.has(c,'choose_dialog',player=1,value=0);c=self.cases(variants=['enchantment_type_1'])[0];s=c['steps'];probe=s.index(dict(op='require_can_cast',ref='type_witness',value=1));self.assertEqual(s[probe-1],dict(op='assert_card',ref='test_token',property='in_play',value=1));self.assertEqual(s[probe-2],dict(op='turn',player=1,value=0));self.assertEqual(s[probe-3],dict(op='assert_stack',value=0))

if __name__=='__main__':unittest.main()

class Wave6CreatorGuardTest(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.o=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards'];cls.rows={r['name']:r for r in registry()}
 def declaration(self,name):
  from token_trigger_creation_spec import wave6_params
  return dict(family='token_trigger_creation' if 'Creature' in self.o[name]['type_line'] else 'token_spell_extension',version=2,params=wave6_params(self.o[name]),oracle_text=self.o[name]['oracle_text'],variants=['create_0'])
 def cases(self,name,modes):
  from recipes import expand as public_expand
  d=self.declaration(name);d['variants']=modes;return public_expand(name,self.rows[name]['csvid'],d,self.o[name],self.rows,oracle_context=self.o)
 def test_wave6_full_source_and_typed_params(self):
  from token_trigger_creation_spec import WAVE6_RULES,WAVE6_BREADTH_RULES,WAVE6_TRIGGER_RULES,WAVE6_ACTIVATION_RULES,WAVE6_DAMAGE_RULES,WAVE6_WOLF_RULES,wave6_spec
  for name in dict(WAVE6_RULES,**WAVE6_BREADTH_RULES,**WAVE6_TRIGGER_RULES,**WAVE6_ACTIVATION_RULES,**WAVE6_DAMAGE_RULES,**WAVE6_WOLF_RULES):
   d=self.declaration(name);wave6_spec(d,self.o[name],d['family'])
   for key in self.o[name]:
    r=copy.deepcopy(self.o[name]);r[key]=None if r[key] is not None else 'mutation'
    with self.assertRaises((ValueError,TypeError,KeyError),msg=(name,key)):wave6_spec(d,r,d['family'])
   def leaves(obj,path=()):
    if isinstance(obj,dict):
     for k,v in obj.items():yield from leaves(v,path+(k,))
    elif isinstance(obj,list):
     for i,v in enumerate(obj):yield from leaves(v,path+(i,))
    else:yield path,obj
   for path,value in leaves(d['params']):
    bad=copy.deepcopy(d);parent=bad['params']
    for key in path[:-1]:parent=parent[key]
    parent[path[-1]]=int(value) if type(value) is bool else True if type(value) is int else 'mutation'
    with self.assertRaises(ValueError,msg=(name,path)):wave6_spec(bad,self.o[name],d['family'])
 def test_wave6_witness_and_token_identity_independence(self):
  from token_trigger_creation_spec import wave6_data,wave6_spec,validate_wave6_witnesses
  name='Myrsmith';d=self.declaration(name);p=wave6_spec(d,self.o[name],d['family']);data=wave6_data()
  for name in data['cards']:
   bad=copy.deepcopy(self.o);bad[name]['oracle_text']+=' altered'
   with self.assertRaises(ValueError,msg=name):validate_wave6_witnesses(bad,self.rows,p)
  rows=copy.deepcopy(self.rows)
  for name in data['registry_identity_only']:rows[name].update(handler='card_dummy',power=99,toughness=99)
  validate_wave6_witnesses(self.o,rows,p)
  rows['Myr']['csvid']+=1
  with self.assertRaises(ValueError):validate_wave6_witnesses(self.o,rows,p)
 def test_wave6_flashback_atomic_and_threshold(self):
  for owner in (0,1):
   c=self.cases('Increasing Devotion',['flashback_'+str(owner)])[0];s=c['steps'];self.assertIn(dict(op='cast_graveyard',ref='source',value=1),s);self.assertNotIn(dict(op='assert_stack',value=1),s);self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='battlefield',index=self.rows['Human']['csvid'],value=10),s)
   c=self.cases('Martial Coup',['create_'+str(owner)])[0];self.assertIn(dict(op='assert_card',ref='ordinary',property='in_play',value=0),c['steps']);self.assertIn(dict(op='assert_card',ref='surviving_land',property='in_play',value=1),c['steps'])
   c=self.cases('Martial Coup',['below_threshold_'+str(owner)])[0];self.assertIn(dict(op='assert_card',ref='ordinary',property='in_play',value=1),c['steps'])
 def test_wave6_real_tap_lifelink_and_static_removal(self):
  for owner in (0,1):
   s=self.cases('Emmara, Soul of the Accord',['lifelink_'+str(owner)])[0]['steps'];self.assertIn(dict(op='assert_life',player=owner,value=21),s);self.assertIn(dict(op='attack',ref='test_token',value=1),s)
   s=self.cases('Master Trinketeer',['static_removed_'+str(owner)])[0]['steps'];self.assertIn(dict(op='destroy',ref='source'),s)
   powers=[x['value'] for x in s if x['op']=='assert_card' and x.get('property')=='power' and 'Servo' in x.get('ref','')];self.assertIn(2,powers);self.assertIn(1,powers)
 def test_wave6_fresh_meat_counts_token_death_and_real_expiry(self):
  for owner in (0,1):
   s=self.cases('Fresh Meat',['token_deaths_'+str(owner)])[0]['steps'];self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='battlefield',index=self.rows['Beast']['csvid'],value=2),s)
   s=self.cases('Fresh Meat',['prior_turn_expired_'+str(owner)])[0]['steps'];self.assertIn(dict(op='advance_turn'),s);self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='battlefield',index=self.rows['Beast']['csvid'],value=0),s)
 def test_wave6_grave_preparation_and_payment_query_contract(self):
  from token_trigger_creation_spec import wave6_spec,wave6_variants
  for owner in (0,1):
   for mode in ('flashback','flashback_denied'):
    s=self.cases('Increasing Devotion',[mode+'_'+str(owner)])[0]['steps'];prep=next(i for i,a in enumerate(s) if a['op']=='prepare_graveyard_abilities');query=next(i for i,a in enumerate(s) if a['op']=='require_can_cast_graveyard');self.assertLess(prep,query)
   s=self.cases('Secure the Wastes',['insufficient_mana_'+str(owner)])[0]['steps'];self.assertIn(dict(op='cast',ref='source',value=0),s);self.assertFalse(any(a['op']=='require_can_cast' for a in s))
  for name in ('Myrsmith','Emmara, Soul of the Accord'):
   d=self.declaration(name);allowed=wave6_variants(wave6_spec(d,self.o[name],d['family']));self.assertNotIn('payment_0',allowed);self.assertNotIn('insufficient_mana_0',allowed)
 def test_wave6_actual_shuffle_receipts_and_x_body(self):
  for owner in (0,1):
   s=self.cases('Beacon of Creation',['create_'+str(owner)])[0]['steps'];self.assertIn(dict(op='assert_shuffle_count',player=owner,value=1),s);self.assertIn(dict(op='assert_shuffle_count',player=1-owner,value=0),s);self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='library',index=self.rows['Beacon of Creation']['csvid'],value=1),s);self.assertFalse(any(a['op']=='assert_zone_card' and a.get('player')==owner for a in s))
   s=self.cases('Gelatinous Genesis',['larger_amount_'+str(owner)])[0]['steps'];self.assertIn(dict(op='choose_x',player=owner,value=10),s);self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='battlefield',index=self.rows['Ooze']['csvid'],value=5),s);self.assertIn(dict(op='assert_card',ref='receipt_'+str(owner)+'_Ooze_0',property='power',value=5),s);self.assertIn(dict(op='assert_mana',player=owner,color=0,value=20),s)
 def test_wave6_haste_then_real_expiry_and_spawn_mana(self):
  for owner in (0,1):
   s=self.cases('Thatcher Revolt',['fresh_attack_'+str(owner)])[0]['steps'];self.assertIn(dict(op='attack',ref='test_token',value=1),s);self.assertFalse(any(a['op']=='advance_turn' for a in s))
   s=self.cases('Thatcher Revolt',['expiry_snapshot_'+str(owner)])[0]['steps'];self.assertEqual(sum(a['op']=='cleanup' for a in s),2);self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='battlefield',index=self.rows['Human']['csvid'],value=0),s)
   s=self.cases('Skittering Invasion',['token_mana_all_'+str(owner)])[0]['steps'];self.assertEqual(sum(a['op']=='activate' and a.get('ref')=='mana_token' for a in s),5);self.assertIn(dict(op='assert_mana',player=owner,color=0,value=28),s)
 def test_wave6_kindred_predicate_arcane_draw_and_optional_tap(self):
  for owner in (0,1):
   c=self.cases('Lys Alana Huntmaster',['predicate_second_type_'+str(owner)])[0];self.assertTrue(any(a['name']=="Eyeblight's Ending" for a in c['setup']));self.assertIn(dict(op='choose_target',player=owner,ref='ordinary'),c['steps']);self.assertIn(dict(op='assert_card',ref='ordinary',property='in_play',value=0),c['steps'])
   c=self.cases('Oyobi, Who Split the Heavens',['predicate_second_type_'+str(owner)])[0];self.assertIn(dict(op='assert_zone',player=owner,zone='library',value=2),c['steps']);self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='hand',index=self.rows['Forest' if owner==0 else 'Plains']['csvid'],value=1),c['steps'])
   s=self.cases('Stonybrook Schoolmaster',['untap_only_'+str(owner)])[0]['steps'];self.assertIn(dict(op='choose_dialog',player=0,value=1),s);self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='battlefield',index=self.rows['Merfolk Wizard']['csvid'],value=0),s)
 def test_wave6_goblinslide_counts_real_removal_spell_feedback(self):
  for owner in (0,1):
   s=self.cases('Goblinslide',['bounce_token_'+str(owner)])[0]['steps'];self.assertIn(dict(op='choose_target',player=owner,ref='removed_token'),s);self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='battlefield',index=self.rows['Goblin']['csvid'],value=1),s)
 def test_wave6_cost_receipts_fresh_non_tap_cost_and_foreign_grave(self):
  for owner in (0,1):
   s=self.cases('Voice of the Woods',['fresh_other_cost_'+str(owner)])[0]['steps'];self.assertFalse(any(a['op']=='advance_turn' for a in s));self.assertEqual(sum(a['op']=='choose_target' and a.get('player')==owner for a in s),5);self.assertIn(dict(op='activate',ref='source',value=1),s)
   s=self.cases('Goblin Trenches',['foreign_cost_'+str(owner)])[0]['steps'];self.assertIn(dict(op='assert_zone_multiplicity',player=1-owner,zone='graveyard',index=self.rows['Forest']['csvid'],value=1),s)
   s=self.cases('Null Caller',['cost_receipt_'+str(owner)])[0]['steps'];i=next(i for i,a in enumerate(s) if a['op']=='activate');j=next(j for j in range(i,len(s)) if s[j]['op']=='resolve');self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='exile',index=self.rows['Ashcoat Bear']['csvid'],value=1),s[i:j])
 def test_wave6_real_land_untap_and_empty_pool_gate(self):
  for owner in (0,1):
   s=self.cases('Memorial to Glory',['create_'+str(owner)])[0]['steps'];u=next(i for i,a in enumerate(s) if a['op']=='untap_step');self.assertEqual(s[u-2:u],[dict(op='advance_turn'),dict(op='advance_turn')]);self.assertTrue(any(a==dict(op='assert_card',ref='source',property='tapped',value=1) for a in s[:u]));self.assertEqual(s[u+1],dict(op='turn',player=owner,value=0))
   for player in (0,1):self.assertTrue(all(dict(op='set_mana',player=player,color=c,value=0) in s[:u] for c in range(6)))
 def test_wave6_mandatory_life_followups_are_operative(self):
  for owner in (0,1):
   s=self.cases('Drogskol Cavalry',['normal_spirit_entry_'+str(owner)])[0]['steps'];self.assertIn(dict(op='assert_life',player=owner,value=24),s)
   s=self.cases('Slimefoot, the Stowaway',['wrath_death_'+str(owner)])[0]['steps'];self.assertIn(dict(op='assert_life',player=owner,value=21),s);self.assertIn(dict(op='assert_life',player=1-owner,value=19),s)
   s=self.cases('Dawn of Hope',['lifelink_draw_'+str(owner)])[0]['steps'];self.assertIn(dict(op='assert_life',player=owner,value=21),s);self.assertIn(dict(op='assert_zone',player=owner,zone='library',value=2),s)
 def test_wave6_tapped_token_and_enchantment_functional_controls(self):
  for owner in (0,1):
   s=self.cases('Null Caller',['tapped_token_block_denied_'+str(owner)])[0]['steps'];self.assertIn(dict(op='block',ref='test_token',other='combat_witness',value=0),s)
   s=self.cases('Renowned Weaver',['enchantment_target_'+str(owner)])[0]['steps'];self.assertIn(dict(op='require_can_cast',ref='type_probe',value=0),s);self.assertIn(dict(op='require_can_cast',ref='type_probe',value=1),s);self.assertIn(dict(op='cast',ref='type_probe',value=1),s)
 def test_wave6_generated_declaration_cannot_mutate_reviewed_whitelist(self):
  from token_trigger_creation_spec import wave6_params
  for name in ('Goblinslide','Drogskol Cavalry','Patrol Signaler'):
   expected=wave6_params(self.o[name]);p=wave6_params(self.o[name]);p['source_keywords'].append('Haste')
   if p['activation']:p['activation']['mana'][0]+=9
   elif p['operative'][0].get('payment'):p['operative'][0]['payment'][0]+=9
   self.assertEqual(wave6_params(self.o[name]),expected)
 def test_wave6_defending_attackers_are_ready_and_life_trigger_payment_isolated(self):
  for name,mode in [('Renowned Weaver','block_flying'),('Null Caller','tapped_token_block_denied')]:
   for owner in (0,1):
    c=self.cases(name,[mode+'_'+str(owner)])[0];w=next(s for s in c['setup'] if s['ref']=='combat_witness');self.assertEqual(w['zone'],'battlefield');self.assertFalse(any(a['op']=='cast' and a.get('ref')=='combat_witness' for a in c['steps']))
  s=self.cases('Dawn of Hope',['normal_lifegain_draw_0'])[0]['steps'];i=next(i for i,a in enumerate(s) if a['op']=='cast' and a.get('ref')=='life_spell');q=next(i for i,a in enumerate(s) if a['op']=='choose_priority' and a.get('ref')=='source');self.assertLess(i,q);self.assertEqual(s[q-1],dict(op='assert_stack',value=1));self.assertTrue(all(dict(op='set_mana',player=0,color=c,value=v) in s[:i] for c,v in enumerate([2,0,0,0,0,1])));self.assertIn(dict(op='assert_mana',player=0,color=0,value=0),s)
 def test_wave6_elf_count_payment_inputs_cannot_supply_denied_green(self):
  for mode in ('payment','insufficient_mana','wrong_color'):
   for owner in (0,1):
    c=self.cases('Elvish Promenade',[mode+'_'+str(owner)])[0];elves=[s for s in c['setup'] if s['name']=='Elvish Mystic'];self.assertTrue(elves);self.assertTrue(all('tapped' in s.get('state',[]) for s in elves))
 def test_wave6_strict_outer_version_and_variant_grammar(self):
  from token_trigger_creation_spec import wave6_spec
  d=self.declaration('Raised by Wolves')
  for value in (2.0,True,'2',None):
   bad=copy.deepcopy(d);bad['version']=value
   with self.assertRaises(ValueError):wave6_spec(bad,self.o['Raised by Wolves'],bad['family'])
  for value in (None,True,'create_0',('create_0',),[],[1],['create_0','create_0']):
   bad=copy.deepcopy(d);bad['variants']=value
   with self.assertRaises(ValueError):wave6_spec(bad,self.o['Raised by Wolves'],bad['family'])
 def test_wave6_damage_quantity_and_actual_first_strike(self):
  for owner in (0,1):
   s=self.cases('Rapacious One',['blocked_'+str(owner)])[0]['steps'];self.assertIn(dict(op='assert_life',player=1-owner,value=17),s);self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='battlefield',index=self.rows['Eldrazi Spawn']['csvid'],value=3),s);self.assertIn(dict(op='assert_card',ref='source',property='damage',value=2),s)
   s=self.cases('Precinct Captain',['blocked_'+str(owner)])[0]['steps'];self.assertIn(dict(op='assert_card',ref='source',property='in_play',value=1),s);self.assertIn(dict(op='assert_card',ref='source',property='damage',value=0),s);self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='battlefield',index=self.rows['Soldier']['csvid'],value=0),s)
 def test_wave6_nested_exact_identity_and_multiple_real_counter_costs(self):
  from token_trigger_creation_spec import wave6_params,wave6_spec,validate_wave6_witnesses
  self.assertNotIn('Phyrexian Zombie',self.o);d=self.declaration('Nested Ghoul');p=wave6_spec(d,self.o['Nested Ghoul'],d['family']);self.assertEqual(p['token'],dict(name='Phyrexian Zombie',power=2,toughness=2,colors=['B'],subtypes=['Phyrexian','Zombie'],artifact=False,keywords=[]));validate_wave6_witnesses(self.o,self.rows,p)
  bad=copy.deepcopy(p);bad['source_name']='Precinct Captain'
  with self.assertRaises(ValueError):validate_wave6_witnesses(self.o,self.rows,bad)
  rows=copy.deepcopy(self.rows);rows['Phyrexian Zombie']['csvid']=18860
  with self.assertRaises(ValueError):validate_wave6_witnesses(self.o,rows,p)
  for owner in (0,1):
   s=self.cases('Nested Ghoul',['multiple_damage_events_'+str(owner)])[0]['steps'];self.assertEqual(sum(a['op']=='activate' and a.get('ref')=='shooter' for a in s),2);self.assertIn(dict(op='assert_card',ref='shooter',property='plus1_counters',value=3),s);self.assertIn(dict(op='assert_card',ref='shooter',property='plus1_counters',value=1),s);self.assertIn(dict(op='assert_matching_permanents',player=owner,index=22242,property='token',value=1),s);self.assertIn(dict(op='assert_matching_permanents',player=owner,index=22242,property='subtype_Phyrexian',value=1),s);self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='battlefield',index=22242,value=2),s)
 def test_wave6_sphinx_actual_noncombat_negative_and_sliver_recipient(self):
  for owner in (0,1):
   s=self.cases('Sharding Sphinx',['noncombat_damage_'+str(owner)])[0]['steps'];self.assertIn(dict(op='choose_player',player=owner,value=1-owner),s);self.assertIn(dict(op='assert_matching_permanents',player=owner,index=self.rows['Triskelion']['csvid'],property='artifact',value=1),s);self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='battlefield',index=self.rows['Thopter']['csvid'],value=0),s)
   if owner==0:self.assertIn(dict(op='assert_unused_choices',value=1),s)
   s=self.cases('Brood Sliver',['opponent_sliver_'+str(owner)])[0]['steps'];self.assertIn(dict(op='assert_zone_multiplicity',player=1-owner,zone='battlefield',index=self.rows['Sliver']['csvid'],value=1),s)
   s=self.cases('Brood Sliver',['foreign_sliver_'+str(owner)])[0]['steps'];self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='battlefield',index=self.rows['Sliver']['csvid'],value=1),s);self.assertIn(dict(op='assert_zone_multiplicity',player=1-owner,zone='graveyard',index=self.rows['Sliver Construct']['csvid'],value=1),s)
 def test_wave6_wolf_counts_morbid_history_and_distinct_bodies(self):
  for owner in (0,1):
   s=self.cases("Predator's Howl",['prior_input_removed_'+str(owner)])[0]['steps'];self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='battlefield',index=self.rows['Wolf']['csvid'],value=3),s)
   s=self.cases("Predator's Howl",['prior_turn_expired_'+str(owner)])[0]['steps'];self.assertIn(dict(op='advance_turn'),s);self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='battlefield',index=self.rows['Wolf']['csvid'],value=1),s);self.assertIn(dict(op='ai_turns',value=2),s);death=next(i for i,a in enumerate(s) if a['op']=='destroy' and a.get('ref')=='history_creature');self.assertFalse(any(a['op']=='reset_death_history' for a in s[death+1:]));self.assertFalse(any(a['op']=='assert_card' and a.get('ref')=='history_creature' for a in s[next(i for i,a in enumerate(s) if a['op']=='ai_turns')+1:]))
   s=self.cases('Bestial Menace',['create_'+str(owner)])[0]['steps']
   for token,power in [('Snake',1),('Wolf',2),('Elephant',3)]:self.assertIn(dict(op='assert_card',ref='receipt_'+str(owner)+'_'+token+'_0',property='power',value=power),s)
 def test_wave6_multikicker_pending_cost_and_reentry(self):
  for owner in (0,1):
   s=self.cases('Wolfbriar Elemental',['payment_'+str(owner)])[0]['steps'];self.assertEqual([a['value'] for a in s if a['op']=='choose_dialog'],[0,0]);self.assertIn(dict(op='assert_mana',player=owner,color=3,value=0),s)
   s=self.cases('Wolfbriar Elemental',['zero_kicker_'+str(owner)])[0]['steps'];self.assertIn(dict(op='choose_dialog',player=owner,value=1),s);self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='battlefield',index=self.rows['Wolf']['csvid'],value=0),s)
   s=self.cases('Wolfbriar Elemental',['kicker_snapshot_'+str(owner)])[0]['steps'];i=next(i for i,a in enumerate(s) if a['op']=='capture');self.assertEqual(s[i-1],dict(op='cast',ref='source',value=1));self.assertEqual(sum(a['op']=='cast' and a.get('ref')=='source' for a in s),1)
   s=self.cases('Wolfbriar Elemental',['bounce_recast_'+str(owner)])[0]['steps'];self.assertEqual(sum(a['op']=='cast' and a.get('ref')=='source' for a in s),2);self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='battlefield',index=self.rows['Wolf']['csvid'],value=3),s)
 def test_wave6_fable_hybrid_choices_dual_colors_and_blue_removal_feedback(self):
  for owner in (0,1):
   for mode in ('create','payment'):
    s=self.cases('Fable of Wolf and Owl',[mode+'_'+str(owner)])[0]['steps'];self.assertTrue(all(dict(op='assert_mana',player=owner,color=c,value=0) in s for c in range(6)));self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='battlefield',index=self.rows['Wolf']['csvid'],value=1),s);self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='battlefield',index=self.rows['Bird']['csvid'],value=1),s)
   s=self.cases('Fable of Wolf and Owl',['bounce_token_'+str(owner)])[0]['steps'];self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='battlefield',index=self.rows['Bird']['csvid'],value=2),s)
 def test_wave6_aura_owns_count_not_bearer_and_actual_grant_loss_damage(self):
  for owner in (0,1):
   s=self.cases('Raised by Wolves',['foreign_wolf_'+str(owner)])[0]['steps'];self.assertIn(dict(op='assert_card',ref='bearer',property='power',value=5),s);self.assertIn(dict(op='assert_card',ref='bearer',property='power',value=4),s);self.assertIn(dict(op='assert_zone_multiplicity',player=1-owner,zone='graveyard',index=self.rows['Watchwolf']['csvid'],value=1),s)
   s=self.cases('Raised by Wolves',['opposing_bearer_'+str(owner)])[0]['steps'];self.assertIn(dict(op='assert_card',ref='bearer',property='power',value=4),s)
   active=self.cases('Raised by Wolves',['bearer_attack_'+str(owner)])[0];lost=self.cases('Raised by Wolves',['aura_lost_attack_'+str(owner)])[0];self.assertEqual(active['setup'],lost['setup']);self.assertIn(dict(op='assert_life',player=1-owner,value=16),active['steps']);self.assertIn(dict(op='assert_life',player=1-owner,value=18),lost['steps'])
 def test_wave6_noncombat_sentinel_and_actual_permanent_witnesses(self):
  from token_trigger_creation_spec import wave6_data
  data=wave6_data();self.assertNotIn('Spore Swarm',data['cards']);self.assertEqual(data['cards']['Yavimaya Sapherd'],self.o['Yavimaya Sapherd']);self.assertEqual(data['cards']['Boomerang'],self.o['Boomerang'])
  s=self.cases('Sharding Sphinx',['noncombat_damage_0'])[0]['steps'];self.assertIn(dict(op='choose_trigger',ref='source'),s);self.assertFalse(any(a['op']=='choose_priority' for a in s))
  for owner in (0,1):
   c=self.cases('Slimefoot, the Stowaway',['opponent_saproling_death_'+str(owner)])[0];self.assertTrue(any(a['name']=='Yavimaya Sapherd' and a['player']==1-owner for a in c['setup']));s=c['steps'];i=next(i for i,a in enumerate(s) if a['op']=='cast' and a.get('ref')=='opposing_saprolings');self.assertIn(dict(op='turn',player=1-owner,value=0),s[:i]);self.assertIn(dict(op='assert_life',player=owner,value=20),s);self.assertIn(dict(op='assert_life',player=1-owner,value=20),s)
   for name,witness in [('Raised by Wolves','Boomerang'),('Wolfbriar Elemental','Unsummon')]:
    c=self.cases(name,['bounce_recast_'+str(owner)])[0];self.assertEqual(next(a['name'] for a in c['setup'] if a['ref']=='bounce_source'),witness)
 def test_wave6_actual_countered_normal_sources_and_lost_aura_target(self):
  from token_trigger_creation_spec import wave6_spec,wave6_variants
  for name in ('Secure the Wastes','Martial Coup','Gelatinous Genesis','Decree of Justice','Beacon of Creation',"Predator's Howl",'Bestial Menace','Wolfbriar Elemental','Raised by Wolves'):
   for owner in (0,1):
    c=self.cases(name,['countered_'+str(owner)])[0];s=c['steps'];self.assertIn(dict(op='choose_priority',ref='counter'),s);self.assertIn(dict(op='cast',ref='source',value=0),s);self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='graveyard',index=self.rows[name]['csvid'],value=1),s);self.assertFalse(any(a['op']=='cast_graveyard' for a in s));self.assertIn(dict(op='assert_card',ref='ordinary',property='in_play',value=1),s)
    if name=='Beacon of Creation':self.assertIn(dict(op='assert_shuffle_count',player=owner,value=0),s)
    if name=='Gelatinous Genesis':self.assertIn(dict(op='choose_x',player=owner,value=6),s)
    if name=='Martial Coup':self.assertIn(dict(op='assert_card',ref='opposing_creature',property='in_play',value=1),s)
  for owner in (0,1):
   c=self.cases('Raised by Wolves',['lost_target_'+str(owner)])[0];s=c['steps'];self.assertIn(dict(op='cast_response',ref='target_removal',value=1),s);self.assertIn(dict(op='assert_stack',value=2),s);self.assertIn(dict(op='resolve_response'),s);self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='battlefield',index=self.rows['Wolf']['csvid'],value=0),s);self.assertIn(dict(op='assert_card',ref='ordinary',property='in_play',value=0),s)
  for name in ('Fable of Wolf and Owl','Goblinslide','Emmara, Soul of the Accord'):
   d=self.declaration(name);self.assertNotIn('countered_0',wave6_variants(wave6_spec(d,self.o[name],d['family'])))
 def test_wave6_selector_contract_and_post_wrath_combat_fixture(self):
  for owner in (0,1):
   for mode in ('create','payment','countered'):
    s=self.cases('Decree of Justice',[mode+'_'+str(owner)])[0]['steps'];self.assertIn(dict(op='choose_x',player=owner,value=3),s)
   self.assertIn(dict(op='choose_x',player=owner,value=6),self.cases('Gelatinous Genesis',['create_'+str(owner)])[0]['steps'])
   for mode in ('block_ground','block_flying'):
    c=self.cases('Martial Coup',[mode+'_'+str(owner)])[0];self.assertEqual(next(a['zone'] for a in c['setup'] if a['ref']=='combat_witness'),'hand');s=c['steps'];i=next(i for i,a in enumerate(s) if a['op']=='cast' and a.get('ref')=='combat_witness');self.assertTrue(any(a['op']=='resolve' for a in s[:i]));self.assertEqual(sum(a['ref']=='combat_witness' for a in c['setup']),1)
   s=self.cases('Khalni Garden',['fresh_attack_'+str(owner)])[0]['steps'];self.assertFalse(any(a['op'] in ('advance_turn','untap_step') for a in s));self.assertIn(dict(op='attack',ref='test_token',value=0),s)
   c=self.cases('Memorial to Glory',['foreign_source_'+str(owner)])[0];self.assertIn('tapped',next(a['state'] for a in c['setup'] if a['ref']=='source'));self.assertIn(dict(op='untap_step',player=owner),c['steps'])
 def test_wave6_actual_optional_payment_and_ai_sacrifice_paths(self):
  c=self.cases('Goblinslide',['create_0'])[0];self.assertFalse(any(a['op']=='choose_priority' for a in c['steps']))
  c=self.cases('Goblinslide',['decline_0'])[0];self.assertTrue(any(a['ref']=='optional_payment_land' and a['name']=='Forest' for a in c['setup']));self.assertIn(dict(op='choose_cancel_mana',player=0),c['steps']);self.assertIn(dict(op='cast',ref='event_spell',value=1),c['steps']);self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='battlefield',index=self.rows['Goblin']['csvid'],value=0),c['steps'])
  for name in ('Goblin Warrens','Goblin Trenches'):
   s=self.cases(name,['create_1'])[0]['steps'];self.assertFalse(any(a['op']=='choose_target' for a in s))
  c=self.cases('Goblin Warrens',['repeat_1'])[0];self.assertEqual(sum(a['ref'].startswith('goblin_cost_') for a in c['setup']),2);self.assertEqual(sum(a['op']=='activate' and a.get('ref')=='source' for a in c['steps']),2);self.assertIn(dict(op='assert_zone_multiplicity',player=1,zone='battlefield',index=self.rows['Goblin']['csvid'],value=1),c['steps']);self.assertIn(dict(op='assert_zone_multiplicity',player=1,zone='battlefield',index=self.rows['Goblin']['csvid'],value=4),c['steps'])
