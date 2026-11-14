import copy
import json
import unittest
from pathlib import Path

from generate_cases import registry
from prevention_recipes import spec, variants, expand, validate_witnesses
from prevention_recipes import _shield_pins


class PreventionRecipesTest(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.oracle=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards']
  cls.rows={r['name']:r for r in registry()}

 def declaration(self,name='Darkness',vs=None,**changes):
  profiles={
   'Darkness':('spell','fog'), 'Divine Light':('spell','own_creatures'),
   'Respite':('spell','fog'), 'Blunt the Assault':('spell','fog'), 'Riot Control':('spell','you'),
   'Dolmen Gate':('static','own_attackers'), 'Mark of Asylum':('static','own_noncombat'),
   'Light of Sanction':('static','own_sources'), "Urza's Armor":('static','armor'),
   'Personal Sanctuary':('static','own_turn'), 'Statecraft':('static','own_combat'),
   'Dawn Elemental':('self','self_all'), 'Armored Transport':('self','self_blockers'),
   'Fog Bank':('self','self_both'), 'Everdawn Champion':('self','self_combat'),
   'Cho-Manno, Revolutionary':('self','self_all'),
   'Gaseous Form':('aura','aura_both'), 'Defang':('aura','aura_all'),
   'Ghostly Possession':('aura','aura_both'),
   'Spore Frog':('activation','fog'), 'Angus Mackenzie':('activation','fog'),
   "Druid's Deliverance":('spell','you_combat'), "Moment's Peace":('spell','fog'),
   "Hunter's Ambush":('spell','nongreen'),'Tanglesap':('spell','nontrample'),
   'Vine Snare':('spell','small'),'Repel the Abominable':('spell','nonhuman'),
  }
  kind,scope=profiles[name]
  params=dict(kind=kind,scope=scope,gain={'Respite':'attackers','Blunt the Assault':'creatures','Riot Control':'opposing_creatures'}.get(name,'none'),
   activation_cost='{G}{W}{U}' if name=='Angus Mackenzie' else '',tap=name=='Angus Mackenzie',sacrifice=name=='Spore Frog',
   keywords=sorted(self.oracle[name]['keywords']) if kind=='self' else ['Flying'] if name=='Ghostly Possession' else [],
   inactive_clauses={"Druid's Deliverance":['populate'],"Moment's Peace":['flashback']}.get(name,[]))
  params.update(changes)
  return dict(family='prevention',version=2,oracle_text=self.oracle[name]['oracle_text'],params=params,
              variants=vs or ['blocked_0'])

 def cases(self,name='Darkness',vs=None,d=None,oracle=None,rows=None):
  o=oracle or self.oracle;decl=d or self.declaration(name,vs)
  return expand(name,self.rows[name]['csvid'],decl,spec(decl,o[name]),decl['variants'],rows or self.rows,o)

 def test_whole_pin_and_normal_identity_fail_closed(self):
  d=self.declaration()
  for fields in (dict(oracle_text='Prevent all damage.'),dict(layout='transform'),dict(card_faces=[{}]),dict(selected_face='front'),dict(type_line='Token Instant')):
   r=dict(self.oracle['Darkness'],**fields)
   with self.assertRaises(ValueError):spec(d,r)
  for text in (self.oracle['Darkness']['oracle_text']+' Draw a card.',self.oracle['Darkness']['oracle_text']+'\n{T}: Add {G}.'):
   r=dict(self.oracle['Darkness'],oracle_text=text)
   with self.assertRaises(ValueError):spec(dict(d,oracle_text=text),r)

 def test_explicit_parameters_reject_types_costs_scopes_and_gaps(self):
  for changes in (dict(scope='own_team'),dict(scope=[]),dict(kind=[]),dict(tap=1),dict(sacrifice=1),dict(keywords=['Flying']),dict(activation_cost='{1}'),dict(inactive_clauses=['unknown']),dict(inactive_clauses=[{}])):
   with self.subTest(changes=changes), self.assertRaises(ValueError):self.cases(d=self.declaration(**changes))
  d=self.declaration();d['params'].pop('inactive_clauses')
  with self.assertRaises(ValueError):self.cases(d=d)

 def test_named_cho_manno_rule_cannot_change_scope(self):
  with self.assertRaises(ValueError):self.cases('Cho-Manno, Revolutionary',d=self.declaration('Cho-Manno, Revolutionary',scope='self_combat'))

 def test_inactive_extra_clauses_are_explicit_and_validated(self):
  for name,gap in (("Moment's Peace",'flashback'),("Druid's Deliverance",'populate')):
   c=self.cases(name)[0]
   self.assertIn(gap,c['_recipe']['untested_components'])
   with self.assertRaises(ValueError):self.cases(name,d=self.declaration(name,inactive_clauses=[]))

 def test_unknown_empty_and_duplicate_variants_rejected(self):
  for selected in ([],['bad_0'],['blocked_0','blocked_0']):
   d=self.declaration();d['variants']=selected
   with self.assertRaises(ValueError):self.cases(d=d)

 def test_real_cast_and_damage_used_for_both_controllers(self):
  for owner in (0,1):
   c=self.cases(vs=[f'blocked_{owner}'])[0];s=c['steps']
   for op in ('cast','begin_attackers','attack','finish_attackers','begin_blockers','block','advance_combat','combat_damage','resolve'):
    self.assertIn(op,[x['op'] for x in s])
   self.assertNotIn('event',[x['op'] for x in s])
   self.assertIn(dict(op='assert_card',ref='attacker',property='in_play',value=1),s)
   self.assertIn(dict(op='assert_card',ref='blocker',property='damage',value=0),s)
   self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='graveyard',index=self.rows['Darkness']['csvid'],value=1),s)

 def test_fog_controls_prove_spell_damage_remains_real(self):
  s=self.cases(vs=['noncombat_player_0'])[0]['steps']
  self.assertIn(dict(op='choose_player',player=1,value=0),s)
  self.assertIn(dict(op='assert_life',player=0,value=17),s)
  s=self.cases(vs=['noncombat_friendly_1'])[0]['steps']
  self.assertIn(dict(op='assert_card',ref='blocker',property='damage',value=3),s)

 def test_source_colors_trample_power_and_human_scope_controls(self):
  for name,positive,negative in (("Hunter's Ambush",'red_source','unblocked'),('Tanglesap','unblocked','trample_source'),('Vine Snare','small_source','unblocked'),('Repel the Abominable','unblocked','human_source')):
   pos,neg=self.cases(name,[positive+'_0',negative+'_0'])
   self.assertIn(dict(op='assert_life',player=0,value=20),pos['steps'])
   amount={'trample_source':3,'human_source':2}.get(negative,7)
   self.assertIn(dict(op='assert_life',player=0,value=20-amount),neg['steps'])

 def test_dolmen_gate_has_a_prevention_sensitive_own_attacker_case(self):
  for owner in (0,1):
   c=self.cases('Dolmen Gate',[f'reverse_blocked_{owner}'])[0]
   self.assertEqual(next(x['player'] for x in c['setup'] if x['ref']=='attacker'),owner)
   self.assertIn(dict(op='assert_card',ref='attacker',property='in_play',value=1),c['steps'])
   self.assertIn(dict(op='assert_card',ref='attacker',property='damage',value=0),c['steps'])
   self.assertIn(dict(op='assert_card',ref='blocker',property='in_play',value=0),c['steps'])

 def test_noncombat_own_sources_and_own_turn_predicates(self):
  for name,positive,negative,damage in (('Light of Sanction','friendly_damage','noncombat_friendly',3),('Mark of Asylum','noncombat_friendly','noncombat_enemy',3)):
   pos,neg=self.cases(name,[positive+'_0',negative+'_0'])
   self.assertIn(dict(op='assert_card',ref='blocker',property='damage',value=0),pos['steps'])
   target='attacker' if negative=='noncombat_enemy' else 'blocker'
   self.assertIn(dict(op='assert_card',ref=target,property='damage',value=damage),neg['steps'])
  pos,neg=self.cases('Personal Sanctuary',['noncombat_player_0','opposing_turn_damage_0'])
  self.assertIn(dict(op='assert_life',player=0,value=20),pos['steps'])
  self.assertIn(dict(op='assert_life',player=0,value=17),neg['steps'])

 def test_one_point_armor_reduces_instead_of_cancelling_packet(self):
  s=self.cases("Urza's Armor",['noncombat_player_0'])[0]['steps']
  self.assertIn(dict(op='assert_life',player=0,value=18),s)

 def test_combat_self_prevention_has_real_noncombat_negative(self):
  s=self.cases('Everdawn Champion',['noncombat_0'])[0]['steps']
  self.assertIn(dict(op='assert_card',ref='source',property='in_play',value=0),s)
  s=self.cases('Dawn Elemental',['noncombat_0'])[0]['steps']
  self.assertIn(dict(op='assert_card',ref='source',property='damage',value=0),s)

 def test_armored_transport_attacks_and_blocks_are_distinguished(self):
  pos,neg=self.cases('Armored Transport',['attacking_0','blocked_0'])
  self.assertIn(dict(op='attack',ref='source',value=1),pos['steps'])
  self.assertIn(dict(op='assert_card',ref='source',property='damage',value=0),pos['steps'])
  self.assertIn(dict(op='assert_card',ref='source',property='in_play',value=0),neg['steps'])

 def test_aura_source_and_target_destinations_and_flying_blocker(self):
  c=self.cases('Ghostly Possession',['blocked_1'])[0]
  self.assertEqual(next(x['name'] for x in c['setup'] if x['ref']=='blocker'),'Air Elemental')
  self.assertIn(dict(op='choose_target',player=1,ref='attacker'),c['steps'])
  c=self.cases('Defang',['blocked_0'])[0]
  self.assertIn(dict(op='assert_card',ref='attacker',property='in_play',value=0),c['steps'])
  self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='graveyard',index=self.rows['Defang']['csvid'],value=1),c['steps'])

 def test_cleanup_expires_spells_but_preserves_continuous_rules(self):
  c=self.cases(vs=['cleanup_0'])[0];s=c['steps']
  restored=s.index(dict(op='restore'))
  self.assertIn(dict(op='combat_damage'),s[:restored])
  self.assertIn(dict(op='assert_life',player=0,value=20),s[:restored])
  self.assertLess(s.index(dict(op='cleanup'),restored),s.index(dict(op='begin_attackers'),restored))
  self.assertIn(dict(op='assert_life',player=0,value=13),s)
  s=self.cases('Dawn Elemental',['cleanup_0'])[0]['steps']
  self.assertIn(dict(op='assert_card',ref='source',property='damage',value=0),s)

 def test_cast_payment_is_exact_and_insufficient_cost_is_rejected(self):
  good,bad=self.cases(vs=['cast_payment_1','insufficient_cast_mana_1'])
  self.assertIn(dict(op='assert_mana',player=1,color=1,value=0),good['steps'])
  self.assertIn(dict(op='cast',ref='source',value=0),bad['steps'])
  self.assertFalse(any(x['op']=='combat_damage' for x in bad['steps']))

 def test_sacrifice_fog_is_legal_while_fresh_and_cost_precedes_effect(self):
  s=self.cases('Spore Frog',['fresh_activation_0'])[0]['steps']
  cut=s.index(dict(op='activate',ref='source',value=1));resolve=s.index(dict(op='resolve'),cut)
  self.assertIn(dict(op='assert_card',ref='source',property='in_play',value=0),s[cut:resolve])
  self.assertIn(dict(op='assert_life',player=0,value=20),s)

 def test_angus_tap_sickness_payment_and_pending_source_loss(self):
  for v in ('tapped','sick','fresh_activation','cast_payment','insufficient_mana','wrong_color'):
   s=self.cases('Angus Mackenzie',[v+'_0'])[0]['steps']
   self.assertIn(dict(op='require_can_activate',ref='source',value=0),s)
   self.assertIn(dict(op='assert_life',player=0,value=13),s)
  s=self.cases('Angus Mackenzie',['source_lost_pending_0'])[0]['steps']
  self.assertIn(dict(op='destroy',ref='source'),s)
  self.assertIn(dict(op='assert_life',player=0,value=20),s)

 def test_life_riders_use_actual_creature_and_attacker_counts(self):
  for name,life in (('Respite',21),('Blunt the Assault',23),('Riot Control',21)):
   s=self.cases(name,['unblocked_0'])[0]['steps']
   self.assertIn(dict(op='assert_life',player=0,value=life),s)

 def test_sorcery_cast_precedes_attackers(self):
  s=self.cases('Divine Light',['blocked_0'])[0]['steps']
  self.assertLess(s.index(dict(op='cast',ref='source',value=1)),s.index(dict(op='begin_attackers')))

 def test_snapshots_replay_real_damage_and_exact_zone_checks(self):
  for c in self.cases(vs=['ready_snapshot_0','resolved_snapshot_1']):
   s=c['steps'];cut=s.index(dict(op='restore'))
   self.assertIn(dict(op='ai_roundtrip'),s)
   self.assertIn(dict(op='assert_recorded_state'),s)
   self.assertTrue(any(x['op']=='assert_zone_card' for x in s[cut:]))
   self.assertTrue(any(x['op']=='assert_zone_multiplicity' and x['zone']=='battlefield' for x in s[cut:]))

 def test_every_damage_witness_field_is_pinned(self):
  for nm,k,val in (('War Mammoth','keywords',[]),('Scaled Wurm','power','6'),('Glory Seeker','type_line','Creature — Soldier'),('Lightning Bolt','oracle_text','It deals 1 damage.')):
   o=copy.deepcopy(self.oracle);o[nm][k]=val
   with self.assertRaises(ValueError):validate_witnesses(o,self.rows)

 def test_handlers_do_not_supply_rules_or_damage_expectations(self):
  rows=copy.deepcopy(self.rows)
  for r in rows.values():r['handler']='card_dummy';r['mana_cost']='{99}'
  self.assertEqual(self.cases(),self.cases(rows=rows))

 def test_each_rule_scope_has_a_prevention_sensitive_witness(self):
  # Disable only the expectation model's prevention predicate. Every enrolled
  # rule shape must change a real damage assertion in at least one witness.
  for name in ('Darkness','Divine Light','Dolmen Gate','Mark of Asylum','Light of Sanction',
       "Urza's Armor",'Personal Sanctuary','Statecraft','Dawn Elemental','Armored Transport',
       'Fog Bank','Everdawn Champion','Cho-Manno, Revolutionary','Gaseous Form','Defang',
       'Ghostly Possession','Spore Frog','Angus Mackenzie',"Druid's Deliverance", "Moment's Peace",
       "Hunter's Ambush",'Tanglesap','Vine Snare','Repel the Abominable'):
   with self.subTest(name=name):
    d=self.declaration(name);p=spec(d,self.oracle[name]);selected=[v for v in variants(p) if not v.startswith(('cleanup_','source_removed_'))]
    normal=expand(name,self.rows[name]['csvid'],d,p,selected,self.rows,self.oracle)
    disabled=expand(name,self.rows[name]['csvid'],d,dict(p,scope='__disabled__'),selected,self.rows,self.oracle)
    self.assertTrue(any(a['steps']!=b['steps'] for a,b in zip(normal,disabled)))

 def test_static_transition_variants_only_use_relevant_combat_predicates(self):
  for name in ('Mark of Asylum','Light of Sanction','Personal Sanctuary',"Urza's Armor",'Dolmen Gate'):
   p=spec(self.declaration(name),self.oracle[name]);vs=variants(p)
   self.assertNotIn('source_removed_0',vs)
   if name!='Dolmen Gate':self.assertNotIn('cleanup_0',vs)

 def shield(self,name='Samite Healer',selected=None,params=None,o=None):
  o=o or self.oracle
  d=dict(family='prevention',version=2,params=copy.deepcopy(_shield_pins()['sources'][name]['params']),
   oracle_text=o[name]['oracle_text'],variants=selected or ['effect_0'])
  if params:d['params'].update(params)
  return expand(name,self.rows[name]['csvid'],d,spec(d,o[name]),d['variants'],self.rows,o)

 def test_shield_extension_exact_amount_cost_scope_and_source_pin(self):
  for params in (dict(amount=True),dict(amount=1.0),dict(amount=2),dict(scope='creature'),dict(tap=1),dict(binding='recipient')):
   with self.subTest(params=params),self.assertRaises(ValueError):self.shield(params=params)
  for field,val in (('mana_cost','{9}'),('oracle_id','changed'),('power','2'),('oracle_text','Prevent damage.')):
   o=copy.deepcopy(self.oracle);o['Samite Healer'][field]=val
   with self.assertRaises(ValueError):self.shield(o=o)

 def test_packet_shields_select_actual_damage_and_charge_costs(self):
  for owner in (0,1):
   s=self.shield('Amulet of Kroog',[f'payment_{owner}'])[0]['steps']
   cast=s.index(dict(op='cast',ref='bolt',value=1));resolve=s.index(dict(op='resolve'),cast)
   self.assertIn(dict(op='choose_priority',ref='source'),s[cast:resolve])
   self.assertIn(dict(op='choose_damage_target',player=owner,ref='recipient'),s[cast:resolve])
   self.assertIn(dict(op='assert_card',ref='source',property='tapped',value=1),s)
   self.assertIn(dict(op='assert_card',ref='recipient',property='damage',value=2),s)
   self.assertIn(dict(op='assert_mana',player=owner,color=0,value=0),s)

 def test_player_damage_packet_uses_the_real_player_recipient(self):
  for owner in (0,1):
   s=self.shield('Conservator',[f'effect_{owner}'])[0]['steps']
   self.assertIn(dict(op='choose_damage_player',player=owner,value=owner),s)
   self.assertIn(dict(op='assert_life',player=owner,value=19),s)

 def test_fixed_amount_shield_is_exhausted_before_second_damage(self):
  s=self.shield(selected=['repeat_damage_0'])[0]['steps']
  self.assertIn(dict(op='assert_card',ref='recipient',property='damage',value=2),s)
  self.assertIn(dict(op='assert_card',ref='recipient',property='damage',value=5),s)
  self.assertEqual(sum(x['op']=='choose_priority' and x.get('ref')=='source' for x in s),1)

 def test_future_shield_amount_is_a_shared_remaining_budget(self):
  s=self.shield('Hold at Bay',['repeat_damage_0'])[0]['steps']
  self.assertEqual(sum(x['op']=='cast' and x.get('ref')=='source' for x in s),1)
  self.assertEqual(sum(x['op']=='cast' and x.get('ref') in ('bolt','second_bolt') for x in s),2)
  self.assertIn(dict(op='assert_card',ref='recipient',property='damage',value=0),s)

 def test_future_shield_expiry_has_positive_damage_before_cleanup(self):
  for name in ('Hold at Bay','Shielded Passage'):
   s=self.shield(name,['cleanup_0'])[0]['steps'];cut=s.index(dict(op='cleanup'))
   self.assertIn(dict(op='cast',ref='bolt',value=1),s[:cut])
   self.assertIn(dict(op='assert_card',ref='recipient',property='damage',value=0),s[:cut])
   self.assertIn(dict(op='cast',ref='second_bolt',value=1),s[cut:])
   self.assertIn(dict(op='assert_card',ref='recipient',property='damage',value=3),s[cut:])

 def test_removal_control_prevents_before_removing_the_source(self):
  s=self.shield(selected=['source_removed_0'])[0]['steps'];cut=s.index(dict(op='cast',ref='removal',value=1))
  self.assertIn(dict(op='assert_card',ref='recipient',property='damage',value=2),s[:cut])
  self.assertIn(dict(op='assert_card',ref='recipient',property='damage',value=5),s[cut:])
  self.assertIn(dict(op='assert_card',ref='source',property='in_play',value=0),s)

 def test_shield_snapshot_both_owners_keep_damage_choices_and_receipts(self):
  for owner in (0,1):
   s=self.shield('Amulet of Kroog',[f'ready_snapshot_{owner}'])[0]['steps']
   choices=[x for x in s if x['op']=='choose_damage_target']
   self.assertEqual(len(choices),2);self.assertTrue(all(x['player']==owner for x in choices))
   receipts=[x for x in s if x['op']=='assert_mana' and x['player']==owner and x['color']==0]
   self.assertEqual(len(receipts),2);self.assertTrue(all(x['value']==18 for x in receipts))

 def test_nonmatching_damage_is_dealt_in_full(self):
  s=self.shield(selected=['other_damage_0'])[0]['steps']
  self.assertIn(dict(op='assert_card',ref='recipient',property='damage',value=2),s)
  self.assertIn(dict(op='assert_card',ref='control',property='damage',value=3),s)

 def test_shield_witness_mutations_fail_closed(self):
  o=copy.deepcopy(self.oracle);o['Lightning Bolt']['oracle_id']='changed'
  with self.assertRaises(ValueError):self.shield(o=o)

 def test_human_packet_activation_passes_only_the_earlier_fast_effects_window(self):
  effect,payment,bare=self.shield(selected=['effect_0','payment_0','bare_zones_0'])
  s=effect['steps'];queued=s.index(dict(op='choose_priority',ref='source'))
  self.assertEqual(s[queued-1],dict(op='choose_priority',player=0,value=-2))
  self.assertFalse(any(x['op']=='choose_priority' and x.get('value')==-2 for c in (payment,bare) for x in c['steps']))
  self.assertFalse(any(x['op']=='choose_priority' and x.get('value')==-2 for x in self.shield(selected=['effect_1'])[0]['steps']))

 def test_packet_legality_is_explicitly_a_gap_not_a_scripted_pass_control(self):
  p=spec(dict(family='prevention',version=2,params=_shield_pins()['sources']['Samite Healer']['params'],oracle_text=self.oracle['Samite Healer']['oracle_text'],variants=['effect_0']),self.oracle['Samite Healer'])
  self.assertFalse(any(v.startswith(('tapped_','sick_','fresh_cast_')) for v in variants(p)))
  self.assertIn('tap and summoning-sickness denial during the live damage window',self.shield()[0]['_recipe']['untested_components'])

 def test_selected_source_combat_prevents_only_the_selected_attacker(self):
  for owner in (0,1):
   s=self.shield('Horn of Deafening',[f'effect_{owner}'])[0]['steps']
   self.assertIn(dict(op='choose_target',player=owner,ref='attacker'),s)
   self.assertIn(dict(op='activate',ref='source',value=1),s)
   self.assertIn(dict(op='attack',ref='unselected',value=1),s)
   self.assertIn(dict(op='assert_life',player=owner,value=17),s)
  s=self.shield('Horn of Deafening',['without_effect_0'])[0]['steps']
  self.assertIn(dict(op='assert_life',player=0,value=10),s)

 def test_selected_attacker_still_receives_the_blockers_real_damage(self):
  s=self.shield('Lady Evangela',['blocked_0'])[0]['steps']
  self.assertIn(dict(op='block',ref='blocker',other='attacker',value=1),s)
  self.assertIn(dict(op='assert_card',ref='attacker',property='in_play',value=0),s)
  self.assertIn(dict(op='assert_card',ref='blocker',property='damage',value=0),s)

 def test_source_combat_denials_query_live_legal_creature_targets(self):
  for v in ('tapped','sick','fresh_cast','activation_insufficient','activation_wrong_color'):
   s=self.shield('Lady Evangela',[v+'_0'])[0]['steps']
   self.assertIn(dict(op='require_can_activate',ref='source',value=0),s)
   self.assertIn(dict(op='activate',ref='source',value=0),s)
   self.assertIn(dict(op='assert_life',player=0,value=10),s)

 def test_source_combat_expiry_has_two_real_combats_and_paid_untapping(self):
  s=self.shield('Warning',['cleanup_0'])[0]['steps'];cut=s.index(dict(op='cleanup'))
  self.assertIn(dict(op='assert_life',player=0,value=17),s[:cut])
  self.assertIn(dict(op='cast',ref='untap',value=1),s[cut:])
  self.assertIn(dict(op='assert_life',player=0,value=7),s[cut:])
  self.assertEqual(sum(x['op']=='combat_damage' for x in s),2)

 def test_source_combat_pending_snapshot_replays_both_controller_costs(self):
  for owner in (0,1):
   s=self.shield('Safeguard',[f'pending_snapshot_{owner}'])[0]['steps'];cut=s.index(dict(op='capture'))
   self.assertIn(dict(op='assert_stack',value=1),s[:cut])
   self.assertIn(dict(op='assert_choices_consumed'),s[:cut])
   self.assertIn(dict(op='assert_mana',player=owner,color=0,value=18),s[cut:])
   self.assertIn(dict(op='assert_mana',player=owner,color=5,value=19),s[cut:])
   self.assertIn(dict(op='assert_recorded_state'),s)

 def test_sacrificing_shield_has_exact_source_destination(self):
  for name in ('Auriok Replica','Sanctum Guardian'):
   s=self.shield(name,['effect_0'])[0]['steps']
   self.assertIn(dict(op='assert_card',ref='source',property='in_play',value=0),s)
   self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='graveyard',index=self.rows[name]['csvid'],value=1),s)
   self.assertIn(dict(op='choose_damage_player' if name=='Auriok Replica' else 'choose_damage_target',player=0,**({'value':0} if name=='Auriok Replica' else {'ref':'recipient'})),s)

 def test_fresh_sacrificing_source_combat_ability_is_legal(self):
  s=self.shield('Resistance Fighter',['fresh_cast_0'])[0]['steps']
  self.assertIn(dict(op='assert_card',ref='source',property='sick',value=1),s)
  self.assertIn(dict(op='require_can_activate',ref='source',value=1),s)
  self.assertIn(dict(op='assert_card',ref='source',property='in_play',value=0),s)
  self.assertIn(dict(op='assert_life',player=0,value=17),s)

 def test_gain_life_uses_actual_prevented_damage_for_both_owners(self):
  for owner in (0,1):
   for name,variant,value in (('Reverse Damage','effect',23),('Reverse Damage','small_damage',22),("Candles' Glow",'effect',23),("Candles' Glow",'small_damage',22)):
    s=self.shield(name,[variant+'_'+str(owner)])[0]['steps']
    self.assertIn(dict(op='assert_life',player=owner,value=value),s)

 def test_life_payment_and_white_mana_are_real_shield_costs(self):
  s=self.shield('Righteous Aura',['payment_0'])[0]['steps']
  self.assertIn(dict(op='set_mana',player=0,color=5,value=1),s)
  self.assertIn(dict(op='assert_mana',player=0,color=5,value=0),s)
  self.assertIn(dict(op='assert_life',player=0,value=18),s)

 def test_modal_shield_bindings_select_the_printed_prevention_mode(self):
  for owner in (0,1):
   s=self.shield('Esper Battlemage',[f'effect_{owner}'])[0]['steps']
   self.assertIn(dict(op='present_ai_dialogs',value=1),s)
   mode=s.index(dict(op='choose_dialog',player=owner,value=0));target=s.index(dict(op='choose_damage_player',player=owner,value=owner))
   self.assertLess(mode,target)
   self.assertIn(dict(op='assert_mana',player=owner,color=5,value=19),s)
   self.assertIn(dict(op='assert_life',player=owner,value=19),s)

 def test_alternate_normal_ability_passes_before_prevention_in_bare_boards(self):
  s=self.shield('Serra Paladin',['bare_zones_0'])[0]['steps'];i=s.index(dict(op='choose_priority',ref='source'))
  self.assertEqual(s[i-1],dict(op='choose_priority',player=0,value=-2))
  s=self.shield('Serra Paladin',['bare_zones_1'])[0]['steps']
  self.assertFalse(any(x['op']=='choose_priority' and x.get('value')==-2 for x in s))

 def test_zero_mana_activation_does_not_invent_an_insufficient_mana_predicate(self):
  p=spec(dict(family='prevention',version=2,params=_shield_pins()['sources']['Resistance Fighter']['params'],oracle_text=self.oracle['Resistance Fighter']['oracle_text'],variants=['effect_0']),self.oracle['Resistance Fighter'])
  self.assertNotIn('activation_insufficient_0',variants(p))

 def test_fixed_seven_point_shield_is_not_misrepresented_as_infinite(self):
  s=self.shield('Hold at Bay',['overflow_damage_0'])[0]['steps']
  self.assertEqual(sum(x['op']=='cast' and x.get('ref') in ('bolt','second_bolt','third_bolt') for x in s),3)
  self.assertIn(dict(op='assert_card',ref='recipient',property='damage',value=2),s)

 def test_hallowed_one_option_menu_is_explicit_for_player_one(self):
  zero,one=self.shield('Hallowed Healer',['effect_0','effect_1'])
  self.assertFalse(any(x['op']=='choose_dialog' for x in zero['steps']))
  s=one['steps'];mode=s.index(dict(op='choose_dialog',player=1,value=0));target=s.index(dict(op='choose_damage_target',player=1,ref='recipient'))
  self.assertLess(mode,target)
  self.assertIn(dict(op='assert_card',ref='recipient',property='damage',value=1),s)


 def test_buyback_choice_follows_damage_target_and_pays_only_when_selected(self):
  for owner in (0,1):
   for name in ('Anoint','Invulnerability'):
    normal,exact,paid=self.shield(name,[f'effect_{owner}',f'payment_{owner}',f'buyback_payment_{owner}'])
    s=normal['steps'];target=next(i for i,x in enumerate(s) if x['op'] in ('choose_damage_target','choose_damage_player'))
    self.assertEqual(s[target+1],dict(op='choose_dialog',player=owner,value=1))
    self.assertFalse(any(x['op']=='choose_dialog' for x in exact['steps']))
    s=paid['steps'];target=next(i for i,x in enumerate(s) if x['op'] in ('choose_damage_target','choose_damage_player'))
    self.assertEqual(s[target+1],dict(op='choose_dialog',player=owner,value=0))
    self.assertIn(dict(op='assert_mana',player=owner,color=0,value=0),s)
    cid=self.rows[name]['csvid']
    self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='hand',index=cid,value=1),s)
    self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='graveyard',index=cid,value=0),s)
    self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='graveyard',index=cid,value=1),exact['steps'])

 def test_bone_mask_prevents_full_damage_with_short_and_empty_libraries(self):
  for owner in (0,1):
   for variant,exiled in (('effect',5),('small_damage',4),('short_library',1),('bare_zones',0)):
    c=self.shield('Bone Mask',[variant+'_'+str(owner)])[0];s=c['steps']
    self.assertIn(dict(op='assert_life',player=owner,value=20),s)
    self.assertIn(dict(op='assert_zone',player=owner,zone='exile',value=exiled),s)
    self.assertIn(dict(op='assert_zone',player=owner,zone='library',value=0 if variant in ('short_library','bare_zones') else 7-(2 if variant=='small_damage' else 3)),s)
    if variant=='short_library':self.assertEqual(c['fixture'],'empty_main')
   s=self.shield('Bone Mask',[f'other_damage_{owner}'])[0]['steps']
   self.assertIn(dict(op='assert_life',player=1-owner,value=17),s)

 def test_dark_sphere_rounds_half_down_with_even_and_distinct_odd_damage(self):
  for owner in (0,1):
   for v,expected in (('effect',18),('small_damage',19),('large_damage',17)):
    s=self.shield('Dark Sphere',[v+'_'+str(owner)])[0]['steps']
    self.assertIn(dict(op='assert_life',player=owner,value=expected),s)
   c=self.shield('Dark Sphere',[f'cast_payment_{owner}'])[0]
   self.assertIn(dict(op='cast',ref='source',value=1),c['steps'])
   self.assertIn(dict(op='assert_mana',player=owner,color=0,value=0),c['steps'])
   d=dict(family='prevention',version=2,params=_shield_pins()['sources']['Dark Sphere']['params'],oracle_text=self.oracle['Dark Sphere']['oracle_text'],variants=['effect_0'])
   self.assertNotIn('insufficient_cast_mana_0',variants(spec(d,self.oracle['Dark Sphere'])))

 def test_balm_and_legionnaire_prevention_modes_pay_real_sacrifice_costs(self):
  for owner in (0,1):
   for name in ('Balm of Restoration','Goblin Legionnaire'):
    s=self.shield(name,[f'effect_{owner}'])[0]['steps']
    self.assertIn(dict(op='choose_dialog',player=owner,value=1),s)
    self.assertIn(dict(op='assert_card',ref='recipient',property='damage',value=1),s)
    self.assertIn(dict(op='assert_life',player=owner,value=20),s)
    self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='graveyard',index=self.rows[name]['csvid'],value=1),s)

 def test_new_linked_witnesses_and_profiles_are_whole_oracle_pinned(self):
  for name in ('Lava Axe','Bone Mask','Dark Sphere','Anoint'):
   o=copy.deepcopy(self.oracle);o[name]['oracle_id']='changed'
   with self.assertRaises(ValueError):self.shield('Bone Mask' if name=='Lava Axe' else name,o=o)



 def test_colored_combat_packet_and_creature_control_use_distinct_recipients(self):
  for owner in (0,1):
   s=self.shield('Circle of Protection: Green',[f'effect_{owner}'])[0]['steps']
   self.assertIn(dict(op='block',ref='blocker',other='unselected',value=1),s)
   self.assertIn(dict(op='choose_damage_player',player=owner,value=owner),s)
   self.assertIn(dict(op='assert_life',player=owner,value=20),s)
   self.assertIn(dict(op='assert_card',ref='blocker',property='damage',value=3),s)
   self.assertIn(dict(op='assert_card',ref='unselected',property='in_play',value=0),s)

 def test_forcefield_leaves_one_real_damage_for_large_and_one_power_sources(self):
  for owner in (0,1):
   for v in ('effect','one_damage'):
    s=self.shield('Forcefield',[v+'_'+str(owner)])[0]['steps']
    self.assertIn(dict(op='assert_life',player=owner,value=19),s)
    self.assertIn(dict(op='assert_card',ref='blocker',property='damage',value=3),s)
   s=self.shield('Forcefield',[f'without_effect_{owner}'])[0]['steps']
   self.assertIn(dict(op='assert_life',player=owner,value=13),s)

 def test_field_prevention_exercises_flying_and_nonflying_damage_together(self):
  for owner in (0,1):
   for name,life in (("Al-abara's Carpet",16),('Scarecrow',13)):
    s=self.shield(name,[f'effect_{owner}'])[0]['steps']
    self.assertIn(dict(op='attack',ref='attacker',value=1),s)
    self.assertIn(dict(op='attack',ref='unselected',value=1),s)
    self.assertIn(dict(op='activate',ref='source',value=1),s)
    self.assertIn(dict(op='assert_life',player=owner,value=life),s)
    self.assertFalse(any(x['op'] in ('choose_target','choose_damage_player') for x in s))

 def test_awe_gain_tracks_actual_prevention_after_real_power_change(self):
  for owner in (0,1):
   for name,expected in (('Awe Strike',27),):
    s=self.shield(name,[f'growth_{owner}'])[0]['steps']
    source=s.index(dict(op='cast',ref='source',value=1));growth=s.index(dict(op='cast',ref='growth',value=1));damage=s.index(dict(op='combat_damage'))
    self.assertLess(source,growth);self.assertLess(growth,damage)
    self.assertIn(dict(op='assert_card',ref='attacker',property='power',value=10),s)
    self.assertIn(dict(op='assert_life',player=owner,value=expected),s)
    self.assertEqual(sum(x==dict(op='choose_target',player=owner,ref='attacker') for x in s),2)

 def test_azorius_split_targets_activate_both_printed_damage_clauses(self):
  for owner in (0,1):
   s=self.shield('Azorius Ploy',[f'split_targets_{owner}'])[0]['steps']
   self.assertIn(dict(op='choose_target',player=owner,ref='attacker'),s)
   self.assertIn(dict(op='choose_target',player=owner,ref='blocker'),s)
   self.assertIn(dict(op='block',ref='blocker',other='unselected',value=1),s)
   self.assertIn(dict(op='assert_card',ref='blocker',property='damage',value=0),s)
   self.assertIn(dict(op='assert_life',player=owner,value=20),s)

 def test_songstitcher_predicate_probes_keep_an_independent_legal_attacker(self):
  role,ability=self.shield('Songstitcher',['wrong_role_0','wrong_ability_0'])
  s=role['steps'];self.assertIn(dict(op='probe_activation_target',ref='source',other='blocker',index=-1,value=0),s)
  self.assertLess(s.index(dict(op='attack',ref='attacker',value=1)),s.index(dict(op='probe_activation_target',ref='source',other='blocker',index=-1,value=0)))
  self.assertEqual(next(x for x in ability['setup'] if x['ref']=='unselected')['name'],'Air Elemental')
  self.assertEqual(next(x for x in ability['setup'] if x['ref']=='attacker')['name'],'Scaled Wurm')
  self.assertIn(dict(op='probe_activation_target',ref='source',other='attacker',index=-1,value=0),ability['steps'])

 def test_extended_combat_pending_replay_preserves_real_mana_and_choice_owners(self):
  for owner in (0,1):
   s=self.shield('Azorius Ploy',[f'pending_snapshot_{owner}'])[0]['steps'];cut=s.index(dict(op='capture'))
   self.assertIn(dict(op='assert_stack',value=1),s[:cut])
   self.assertEqual(sum(x['op']=='choose_target' for x in s),2)
   self.assertTrue(all(x['player']==owner for x in s if x['op']=='choose_target'))
   for color,value in ((0,19),(2,19),(5,18)):
    self.assertEqual(sum(x==dict(op='assert_mana',player=owner,color=color,value=value) for x in s),2)



 def test_dazzling_active_scope_is_two_fatal_legal_cast_permissions(self):
  name='Dazzling Reflection';d=dict(family='prevention',version=2,params=_shield_pins()['sources'][name]['params'],oracle_text=self.oracle[name]['oracle_text'],variants=['cast_permission_0'])
  p=spec(d,self.oracle[name]);self.assertEqual(variants(p),['cast_permission_0','cast_permission_1'])
  for owner in (0,1):
   s=self.shield(name,[f'cast_permission_{owner}'])[0]['steps']
   gate=s.index(dict(op='require_can_cast',ref='source',value=1));target=s.index(dict(op='choose_target',player=owner,ref='attacker'))
   self.assertLess(gate,target);self.assertIn(dict(op='cast',ref='source',value=1),s)
   self.assertFalse(any(x['op']=='combat_damage' for x in s))

 def test_energy_field_selected_boards_keep_sacrifice_trigger_inactive(self):
  name='Energy Field';d=dict(family='prevention',version=2,params=_shield_pins()['sources'][name]['params'],oracle_text=self.oracle[name]['oracle_text'],variants=['effect_0'])
  p=spec(d,self.oracle[name]);vs=variants(p)
  self.assertFalse(any(v.startswith(('blocked_','reverse_blocked_','friendly_damage_')) for v in vs))
  for c in expand(name,self.rows[name]['csvid'],d,p,vs,self.rows,self.oracle):
   owner=int(c['_recipe']['variant'][-1]);values=[x['value'] for x in c['steps'] if x['op']=='assert_zone' and x['player']==owner and x['zone']=='graveyard']
   self.assertTrue(all(x in (0,4) for x in values))

 def test_orbs_does_not_cast_an_illegal_opposing_player_targeted_spell(self):
  name='Orbs of Warding';d=dict(family='prevention',version=2,params=_shield_pins()['sources'][name]['params'],oracle_text=self.oracle[name]['oracle_text'],variants=['unblocked_0'])
  p=spec(d,self.oracle[name]);vs=variants(p)
  self.assertNotIn('noncombat_player_0',vs);self.assertNotIn('opposing_turn_damage_0',vs)
  for c in expand(name,self.rows[name]['csvid'],d,p,vs,self.rows,self.oracle):self.assertFalse(any(x['op']=='choose_player' for x in c['steps']))

 def test_armament_and_sivvi_use_the_printed_inactive_clause_boundaries(self):
  c=self.shield('Armament of Nyx',['blocked_0'])[0]
  self.assertEqual(next(x for x in c['setup'] if x['ref']=='attacker')['name'],'Scaled Wurm')
  self.assertIn(dict(op='assert_card',ref='blocker',property='damage',value=0),c['steps'])
  c=self.shield("Sivvi's Ruse",['cast_payment_0'])[0]
  self.assertFalse(any('Land' in self.oracle[x['name']]['type_line'] for x in c['setup'] if x['zone']=='battlefield'))
  self.assertIn(dict(op='set_mana',player=0,color=0,value=2),c['steps']);self.assertIn(dict(op='set_mana',player=0,color=5,value=2),c['steps'])

 def test_hypochondria_sacrifice_preserves_the_available_discard_witness(self):
  for owner in (0,1):
   c=self.shield('Hypochondria',[f'bare_zones_{owner}'])[0];s=c['steps']
   self.assertIn(dict(op='choose_dialog',player=owner,value=1),s)
   self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='hand',index=self.rows['Plains']['csvid'],value=1),s)
   self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='graveyard',index=self.rows['Hypochondria']['csvid'],value=1),s)

 def test_intervention_pact_casts_for_zero_and_gains_prevented_damage(self):
  for owner in (0,1):
   s=self.shield('Intervention Pact',[f'cast_payment_{owner}'])[0]['steps']
   self.assertIn(dict(op='choose_priority',ref='source'),s)
   self.assertIn(dict(op='assert_life',player=owner,value=23),s)
   self.assertIn(dict(op='assert_mana',player=owner,color=0,value=0),s)

 def test_greater_realm_covers_black_and_red_printed_source_choices(self):
  for v,n in (('effect','Scathe Zombies'),('alternate_source','Hill Giant')):
   c=self.shield('Greater Realm of Preservation',[v+'_0'])[0]
   self.assertEqual(next(x for x in c['setup'] if x['ref']=='attacker')['name'],n)
   self.assertIn(dict(op='assert_life',player=0,value=20),c['steps'])



 def test_moonmist_preserves_real_wolf_damage_and_uses_no_human_transform_target(self):
  for owner in (0,1):
   c=self.shield('Moonmist',[f'effect_{owner}'])[0]
   self.assertEqual(next(x for x in c['setup'] if x['ref']=='unselected')['name'],'Watchwolf')
   self.assertIn(dict(op='assert_life',player=owner,value=17),c['steps'])
   self.assertFalse(any('Human' in self.oracle[x['name']]['type_line'] for x in c['setup'] if x['zone']=='battlefield'))

 def test_snag_distinguishes_blocked_damage_from_real_unblocked_prevention(self):
  s=self.shield('Snag',['blocked_0'])[0]['steps']
  self.assertIn(dict(op='assert_card',ref='attacker',property='in_play',value=0),s)
  self.assertIn(dict(op='assert_card',ref='blocker',property='in_play',value=0),s)
  self.assertIn(dict(op='assert_life',player=0,value=20),s)

 def test_winds_snapshots_ferocious_before_real_later_power_reduction(self):
  for owner in (0,1):
   s=self.shield('Winds of Qal Sisma',[f'lose_ferocious_{owner}'])[0]['steps']
   source=s.index(dict(op='cast',ref='source',value=1));shrink=s.index(dict(op='cast',ref='shrink',value=1));damage=s.index(dict(op='combat_damage'))
   self.assertLess(source,shrink);self.assertLess(shrink,damage)
   self.assertIn(dict(op='assert_card',ref='blocker',property='power',value=2),s)
   self.assertIn(dict(op='assert_card',ref='attacker',property='damage',value=2),s)
   s=self.shield('Winds of Qal Sisma',[f'no_ferocious_{owner}'])[0]['steps']
   self.assertIn(dict(op='assert_card',ref='attacker',property='damage',value=0),s)

 def test_terrifying_presence_preserves_the_chosen_creatures_real_damage(self):
  s=self.shield('Terrifying Presence',['effect_0'])[0]['steps']
  self.assertIn(dict(op='choose_target',player=0,ref='attacker'),s)
  self.assertIn(dict(op='assert_life',player=0,value=13),s)
  s=self.shield('Terrifying Presence',['blocked_0'])[0]['steps']
  self.assertIn(dict(op='assert_card',ref='blocker',property='in_play',value=0),s)
  self.assertIn(dict(op='assert_card',ref='attacker',property='damage',value=0),s)

 def test_soul_parry_uses_one_or_two_real_targets_without_inventing_a_second(self):
  for owner in (0,1):
   one,two=self.shield('Soul Parry',[f'effect_{owner}',f'two_targets_{owner}'])
   self.assertIn(dict(op='choose_cancel_target',player=owner),one['steps'])
   self.assertIn(dict(op='choose_target',player=owner,ref='unselected'),two['steps'])
   self.assertIn(dict(op='assert_life',player=owner,value=17),one['steps'])
   self.assertIn(dict(op='assert_life',player=owner,value=20),two['steps'])

 def test_gossamer_returns_source_as_cost_and_probes_blocked_target_denial(self):
  for owner in (0,1):
   s=self.shield('Gossamer Chains',[f'payment_{owner}'])[0]['steps']
   self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='hand',index=self.rows['Gossamer Chains']['csvid'],value=1),s)
   self.assertIn(dict(op='assert_card',ref='source',property='in_play',value=0),s)
   self.assertIn(dict(op='assert_mana',player=owner,color=0,value=0),s)
   s=self.shield('Gossamer Chains',[f'wrong_role_{owner}'])[0]['steps']
   self.assertIn(dict(op='block',ref='blocker',other='attacker',value=1),s)
   self.assertIn(dict(op='probe_activation_target',ref='source',other='attacker',index=-1,value=0),s)

 def test_boros_red_payment_and_linked_damage_are_both_observed(self):
  for owner in (0,1):
   normal,red=self.shield('Boros Fury-Shield',[f'effect_{owner}',f'red_spent_{owner}'])
   self.assertIn(dict(op='set_mana',player=owner,color=4,value=0),normal['steps'])
   self.assertIn(dict(op='assert_mana',player=owner,color=0,value=18),normal['steps'])
   self.assertIn(dict(op='assert_life',player=1-owner,value=20),normal['steps'])
   self.assertIn(dict(op='set_mana',player=owner,color=4,value=1),red['steps'])
   self.assertIn(dict(op='assert_mana',player=owner,color=0,value=19),red['steps'])
   self.assertIn(dict(op='assert_mana',player=owner,color=4,value=0),red['steps'])
   self.assertIn(dict(op='assert_life',player=1-owner,value=13),red['steps'])
   self.assertEqual([x['value'] for x in red['steps'] if x['op']=='choose_dialog'],[1] if owner==0 else [])


 def replacement(self,name='Phantom Nomad',vs=None):
  profile=_shield_pins()['sources'][name]
  d=dict(family='prevention',version=2,params=copy.deepcopy(profile['params']),oracle_text=self.oracle[name]['oracle_text'],variants=vs or ['effect_0'])
  return self.cases(name,d=d)

 def test_replacement_whole_pin_and_parameters_fail_closed(self):
  name='Phantom Nomad';profile=_shield_pins()['sources'][name]
  d=dict(family='prevention',version=2,params=copy.deepcopy(profile['params']),oracle_text=self.oracle[name]['oracle_text'],variants=['effect_0'])
  for changes in (dict(power='1'),dict(mana_cost='{W}'),dict(oracle_text=d['oracle_text']+' Draw a card.'),dict(layout='token')):
   with self.assertRaises(ValueError):spec(d,dict(self.oracle[name],**changes))
  for key,value in (('entry_counters',3),('replacement','draw'),('casting_cost','{W}'),('entry_counters',True),('gaps',[])):
   bad=copy.deepcopy(d);bad['params'][key]=value
   if bad!=d:
    with self.assertRaises(ValueError):spec(bad,self.oracle[name])
  for vs in ([],['invented_0'],['effect_0','effect_0']):
   bad=copy.deepcopy(d);bad['variants']=vs
   with self.assertRaises(ValueError):expand(name,self.rows[name]['csvid'],bad,spec(bad,self.oracle[name]),vs,self.rows,self.oracle)

 def test_replacement_real_entry_cast_and_packet_for_both_owners(self):
  for owner in (0,1):
   s=self.replacement(vs=[f'effect_{owner}'])[0]['steps']
   self.assertIn(dict(op='cast',ref='source',value=1),s)
   self.assertIn(dict(op='cast',ref='packet_0',value=1),s)
   self.assertIn(dict(op='choose_target',player=1-owner,ref='source'),s)
   counters=[x['value'] for x in s if x.get('property')=='plus1_counters' and x.get('ref')=='source']
   self.assertIn(2,counters);self.assertIn(1,counters)
   self.assertNotIn('event',[x['op'] for x in s])

 def test_phantom_zero_counters_remains_protected_after_real_growth(self):
  for owner in (0,1):
   s=self.replacement(vs=[f'zero_counters_{owner}'])[0]['steps']
   self.assertIn(dict(op='cast',ref='growth',value=1),s)
   self.assertEqual([x['ref'] for x in s if x['op']=='cast' and x['ref'].startswith('packet_')],['packet_0','packet_1','packet_2'])
   self.assertGreaterEqual(sum(x==dict(op='assert_card',ref='source',property='plus1_counters',value=0) for x in s),2)
   self.assertIn(dict(op='assert_card',ref='source',property='toughness',value=3),s)
   self.assertEqual([x['value'] for x in s if x.get('property')=='damage' and x.get('ref')=='source'],[0]*6)

 def test_phantom_exhaustion_dies_after_last_prevented_packet(self):
  for owner in (0,1):
   s=self.replacement(vs=[f'exhaustion_{owner}'])[0]['steps']
   self.assertIn(dict(op='assert_card',ref='source',property='in_play',value=0),s)
   self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='graveyard',index=self.rows['Phantom Nomad']['csvid'],value=1),s)

 def test_hydra_adds_damage_quantity_minus_counters_without_marked_damage(self):
  s=self.replacement('Phyrexian Hydra',['repeat_damage_0'])[0]['steps']
  self.assertIn(dict(op='assert_card',ref='source',property='minus1_counters',value=3),s)
  self.assertIn(dict(op='assert_card',ref='source',property='minus1_counters',value=6),s)
  self.assertIn(dict(op='assert_card',ref='source',property='toughness',value=1),s)
  self.assertTrue(all(x['value']==0 for x in s if x.get('ref')=='source' and x.get('property')=='damage'))

 def test_swans_draws_for_actual_damage_source_controller_with_exact_order(self):
  for owner in (0,1):
   for v,caster in (('effect',1-owner),('friendly_source',owner)):
    c=self.replacement('Swans of Bryn Argoll',[f'{v}_{owner}'])[0];s=c['steps']
    self.assertIn(dict(op='choose_target',player=caster,ref='source'),s)
    # The first three real library cards have entered this controller's hand.
    self.assertIn(dict(op='assert_zone',player=caster,zone='library',value=4),s)
    self.assertIn(dict(op='assert_zone_card',player=caster,zone='library',index=0,value=self.rows['Forest']['csvid']),s)
    self.assertIn(dict(op='assert_zone_multiplicity',player=caster,zone='hand',index=self.rows['Forest']['csvid'],value=1),s)

 def test_replacement_cleanup_has_positive_packet_before_and_after(self):
  for owner in (0,1):
   s=self.replacement(vs=[f'cleanup_{owner}'])[0]['steps'];cut=next(i for i,x in enumerate(s) if x['op']=='cleanup')
   self.assertIn(dict(op='cast',ref='packet_0',value=1),s[:cut])
   self.assertIn(dict(op='assert_card',ref='source',property='plus1_counters',value=1),s[:cut])
   self.assertIn(dict(op='cast',ref='packet_1',value=1),s[cut:])

 def test_replacement_unrelated_recipient_receives_real_damage(self):
  s=self.replacement(vs=['other_damage_1'])[0]['steps']
  self.assertIn(dict(op='choose_target',player=0,ref='control'),s)
  self.assertIn(dict(op='cast',ref='control_packet',value=1),s)
  self.assertIn(dict(op='assert_card',ref='control',property='damage',value=3),s)

 def test_replacement_snapshot_replays_both_owner_choices_and_payments(self):
  for owner in (0,1):
   for v in ('ready_snapshot','pending_snapshot','damaged_snapshot'):
    s=self.replacement(vs=[f'{v}_{owner}'])[0]['steps'];restore=next(i for i,x in enumerate(s) if x['op']=='restore')
    for segment in (s[:restore],s[restore:]):
     self.assertIn(dict(op='choose_target',player=1-owner,ref='source'),segment)
     self.assertIn(dict(op='assert_mana',player=owner,color=5,value=19),segment)
     self.assertIn(dict(op='assert_choices_consumed'),segment)


 def test_modifier_whole_pin_and_rule_parameters_fail_closed(self):
  name='Vigor';profile=_shield_pins()['sources'][name]
  d=dict(family='prevention',version=2,params=copy.deepcopy(profile['params']),oracle_text=self.oracle[name]['oracle_text'],variants=['effect_0'])
  for key,value in (('power','7'),('oracle_text','Prevent all damage.'),('colors',['W'])):
   with self.assertRaises(ValueError):spec(d,dict(self.oracle[name],**{key:value}))
  bad=copy.deepcopy(d);bad['params']['damage_rule']='spell_one'
  with self.assertRaises(ValueError):spec(bad,self.oracle[name])
  bad=copy.deepcopy(d);bad['params']['extra']=True
  with self.assertRaises(ValueError):spec(bad,self.oracle[name])
  for vs in ([],['effect_9'],['effect_0','effect_0']):
   bad=dict(d,variants=vs)
   with self.assertRaises(ValueError):expand(name,self.rows[name]['csvid'],bad,spec(bad,self.oracle[name]),vs,self.rows,self.oracle)

 def test_vigor_grows_only_other_controlled_creatures(self):
  for owner in (0,1):
   s=self.replacement('Vigor',[f'effect_{owner}'])[0]['steps']
   self.assertIn(dict(op='assert_card',ref='friendly',property='plus1_counters',value=3),s)
   self.assertIn(dict(op='assert_card',ref='friendly',property='power',value=10),s)
   self.assertTrue(all(x['value']==0 for x in s if x.get('ref')=='friendly' and x.get('property')=='damage'))
   s=self.replacement('Vigor',[f'source_damage_{owner}'])[0]['steps']
   self.assertIn(dict(op='assert_card',ref='source',property='damage',value=3),s)
   self.assertTrue(all(x['value']==0 for x in s if x.get('ref')=='source' and x.get('property')=='plus1_counters'))
   s=self.replacement('Vigor',[f'opponent_creature_{owner}'])[0]['steps']
   self.assertIn(dict(op='assert_card',ref='enemy',property='damage',value=3),s)

 def test_purity_prevents_noncombat_with_mandatory_gain_and_real_combat_control(self):
  for owner in (0,1):
   s=self.replacement('Purity',[f'effect_{owner}'])[0]['steps']
   self.assertIn(dict(op='choose_player',player=1-owner,value=owner),s)
   self.assertIn(dict(op='assert_life',player=owner,value=23),s)
   s=self.replacement('Purity',[f'friendly_source_{owner}'])[0]['steps']
   self.assertIn(dict(op='choose_player',player=owner,value=owner),s)
   self.assertIn(dict(op='assert_life',player=owner,value=23),s)
   s=self.replacement('Purity',[f'creature_damage_{owner}'])[0]['steps']
   self.assertIn(dict(op='combat_damage'),s)
   self.assertIn(dict(op='assert_life',player=owner,value=17),s)

 def test_gisela_controller_predicate_and_odd_even_rounding(self):
  for owner in (0,1):
   for v in ('effect','friendly_source','small_damage'):
    s=self.replacement('Gisela, Blade of Goldnight',[f'{v}_{owner}'])[0]['steps']
    self.assertIn(dict(op='assert_card',ref='friendly',property='damage',value=1),s)
   s=self.replacement('Gisela, Blade of Goldnight',[f'opponent_player_{owner}'])[0]['steps']
   self.assertIn(dict(op='assert_life',player=1-owner,value=14),s)
   s=self.replacement('Gisela, Blade of Goldnight',[f'opponent_creature_{owner}'])[0]['steps']
   self.assertIn(dict(op='assert_card',ref='enemy',property='in_play',value=0),s)

 def test_pegasus_reduces_spell_damage_globally_but_leaves_combat(self):
  for owner in (0,1):
   for v,ref in (('effect','friendly'),('opponent_creature','enemy')):
    s=self.replacement('Plated Pegasus',[f'{v}_{owner}'])[0]['steps']
    self.assertIn(dict(op='assert_card',ref=ref,property='damage',value=2),s)
   s=self.replacement('Plated Pegasus',[f'creature_damage_{owner}'])[0]['steps']
   self.assertIn(dict(op='block',ref='friendly',other='attacker',value=1),s)
   self.assertIn(dict(op='assert_card',ref='friendly',property='damage',value=3),s)

 def test_modifier_cleanup_and_source_removal_have_positive_first(self):
  for owner in (0,1):
   s=self.replacement('Vigor',[f'cleanup_{owner}'])[0]['steps'];cut=next(i for i,x in enumerate(s) if x['op']=='cleanup')
   self.assertIn(dict(op='assert_card',ref='friendly',property='plus1_counters',value=3),s[:cut])
   self.assertIn(dict(op='assert_card',ref='friendly',property='plus1_counters',value=6),s[cut:])
   s=self.replacement('Vigor',[f'source_removed_{owner}'])[0]['steps'];cut=next(i for i,x in enumerate(s) if x.get('op')=='cast' and x.get('ref')=='bounce')
   self.assertIn(dict(op='assert_card',ref='friendly',property='plus1_counters',value=3),s[:cut])
   self.assertIn(dict(op='assert_card',ref='friendly',property='damage',value=3),s[cut:])
   self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='hand',index=self.rows['Vigor']['csvid'],value=1),s)

 def test_modifier_pending_and_damaged_snapshots_keep_owner_choices(self):
  for owner in (0,1):
   for v in ('ready_snapshot','pending_snapshot','damaged_snapshot'):
    s=self.replacement('Vigor',[f'{v}_{owner}'])[0]['steps'];cut=next(i for i,x in enumerate(s) if x['op']=='restore')
    for segment in (s[:cut],s[cut:]):
     self.assertIn(dict(op='choose_target',player=1-owner,ref='friendly'),segment)
     self.assertIn(dict(op='assert_mana',player=owner,color=0,value=17),segment)
     self.assertIn(dict(op='assert_choices_consumed'),segment)


 def test_replacement_fresh_cast_can_really_block_and_still_prevents(self):
  for owner in (0,1):
   s=self.replacement('Phantom Wurm',[f'blocking_{owner}'])[0]['steps']
   self.assertLess(s.index(dict(op='cast',ref='source',value=1)),s.index(dict(op='block',ref='source',other='attacker',value=1)))
   self.assertIn(dict(op='combat_damage'),s)
   self.assertIn(dict(op='assert_card',ref='source',property='plus1_counters',value=3),s)
   self.assertIn(dict(op='assert_card',ref='source',property='damage',value=0),s)
   self.assertIn(dict(op='assert_card',ref='attacker',property='in_play',value=0),s)

 def test_nishoba_gains_initial_assigned_damage_after_counter_removed(self):
  for owner in (0,1):
   s=self.replacement('Phantom Nishoba',[f'blocking_{owner}'])[0]['steps']
   self.assertIn(dict(op='assert_card',ref='source',property='plus1_counters',value=6),s)
   self.assertIn(dict(op='assert_card',ref='source',property='power',value=6),s)
   self.assertIn(dict(op='assert_life',player=owner,value=27),s)
   self.assertNotIn(dict(op='assert_life',player=owner,value=26),s)

 def test_hydra_infect_packet_survives_its_own_simultaneous_death(self):
  for owner in (0,1):
   c=self.replacement('Phyrexian Hydra',[f'blocking_{owner}'])[0];s=c['steps']
   self.assertEqual(next(x['name'] for x in c['setup'] if x['ref']=='attacker'),'Krosan Colossus')
   self.assertIn(dict(op='assert_card',ref='source',property='in_play',value=0),s)
   for prop,value in (('minus1_counters',7),('power',2),('toughness',2),('damage',0)):
    self.assertIn(dict(op='assert_card',ref='attacker',property=prop,value=value),s)
   self.assertIn('Poison to players and counter cancellation',c['_recipe']['untested_components'])
   self.assertNotIn('Outgoing infect damage, poison and counter cancellation',c['_recipe']['untested_components'])

 def test_swans_combat_draw_and_replacement_replay_are_real_for_both_owners(self):
  for owner in (0,1):
   s=self.replacement('Swans of Bryn Argoll',[f'combat_snapshot_{owner}'])[0]['steps'];cut=next(i for i,x in enumerate(s) if x['op']=='restore')
   for part in (s[:cut],s[cut:]):
    self.assertIn(dict(op='block',ref='source',other='attacker',value=1),part)
    self.assertIn(dict(op='assert_zone',player=1-owner,zone='library',value=4),part)
    self.assertIn(dict(op='assert_card',ref='source',property='damage',value=0),part)
    self.assertIn(dict(op='assert_card',ref='attacker',property='in_play',value=0),part)


 def test_selected_source_pays_non_T_tap_cost_even_when_fresh(self):
  for name in ('Field Surgeon','Master Apothecary'):
   for owner in (0,1):
    s=self.shield(name,[f'fresh_tap_cost_{owner}'])[0]['steps']
    self.assertIn(dict(op='cast',ref='source',value=1),s)
    self.assertIn(dict(op='assert_card',ref='source',property='sick',value=1),s)
    first=s.index(dict(op='choose_target',player=owner,ref='source'));second=s.index(dict(op='choose_damage_target',player=owner,ref='recipient'))
    self.assertLess(first,second)
    self.assertIn(dict(op='assert_card',ref='source',property='tapped',value=1),s)
    self.assertNotIn(dict(op='activate',ref='source',value=0),s)

 def test_pearl_shard_distinguishes_real_white_and_generic_payment_modes(self):
  for owner in (0,1):
   normal=self.shield('Pearl Shard',[f'effect_{owner}'])[0]['steps']
   self.assertIn(dict(op='choose_dialog',player=owner,value=0),normal)
   self.assertIn(dict(op='assert_mana',player=owner,color=5,value=19),normal)
   for v in ('payment','alternative_payment'):
    s=self.shield('Pearl Shard',[f'{v}_{owner}'])[0]['steps']
    self.assertFalse(any(x['op']=='choose_dialog' for x in s))
    self.assertIn(dict(op='assert_mana',player=owner,color=0,value=0),s)
    self.assertIn(dict(op='assert_mana',player=owner,color=5,value=0),s)
    self.assertIn(dict(op='set_mana',player=owner,color=0,value=3 if v=='alternative_payment' else 0),s)
    self.assertIn(dict(op='choose_priority',ref='source'),s)
    self.assertIn(dict(op='assert_card',ref='recipient',property='damage',value=1),s)

 def test_tap_repeat_uses_prepared_real_untap_steps_with_empty_pools(self):
  for name in ('Field Surgeon','Master Apothecary','Pearl Shard'):
   for owner in (0,1):
    s=self.shield(name,[f'untap_repeat_{owner}'])[0]['steps'];visits=[(i,x['player']) for i,x in enumerate(s) if x['op']=='untap_step']
    self.assertEqual([pl for _,pl in visits],[1-owner,owner])
    pool=[[None]*6,[None]*6]
    for i,x in enumerate(s):
     if x['op']=='set_mana':pool[x['player']][x['color']]=x['value']
     if x['op']=='untap_step':
      self.assertEqual(s[i-1],dict(op='advance_turn'))
      self.assertEqual(pool,[[0]*6,[0]*6])
    first=next(i for i,x in enumerate(s) if x['op']=='advance_turn')
    self.assertIn(dict(op='assert_card',ref='source',property='tapped',value=1),s[:first])
    self.assertIn(dict(op='assert_card',ref='source',property='tapped',value=0),s[visits[-1][0]:])
    self.assertEqual(sum(x==dict(op='choose_priority',ref='source') for x in s),2)
    self.assertEqual(sum(x['op']=='cast' and x['ref'] in ('bolt','second_bolt') for x in s),2)
    self.assertFalse(any(x['op']=='turn' for x in s[first:visits[-1][0]+1]))

 def test_tap_repeat_snapshot_replays_both_owner_cost_decisions(self):
  for owner in (0,1):
   s=self.shield('Master Apothecary',[f'untap_repeat_snapshot_{owner}'])[0]['steps'];cut=next(i for i,x in enumerate(s) if x['op']=='restore')
   for part in (s[:cut],s[cut:]):
    self.assertEqual(sum(x==dict(op='choose_target',player=owner,ref='source') for x in part),2)
    self.assertEqual([x['player'] for x in part if x['op']=='untap_step'],[1-owner,owner])
    self.assertIn(dict(op='assert_card',ref='recipient',property='damage',value=1),part)

 def test_oriss_is_a_proactive_whole_turn_recipient_activation(self):
  for owner in (0,1):
   s=self.shield('Oriss, Samite Guardian',[f'repeat_damage_{owner}'])[0]['steps']
   self.assertLess(s.index(dict(op='choose_dialog',player=owner,value=0)),s.index(dict(op='choose_target',player=owner,ref='recipient')))
   self.assertLess(s.index(dict(op='activate',ref='source',value=1)),s.index(dict(op='cast',ref='bolt',value=1)))
   self.assertEqual(sum(x==dict(op='activate',ref='source',value=1) for x in s),1)
   self.assertTrue(all(x['value']==0 for x in s if x.get('ref')=='recipient' and x.get('property')=='damage'))
   self.assertFalse(any(x['op']=='choose_priority' for x in s))

 def test_oriss_normal_window_denials_and_persistent_shield_after_source_bounce(self):
  for owner in (0,1):
   for v in ('tapped','sick','fresh_cast'):
    s=self.shield('Oriss, Samite Guardian',[f'{v}_{owner}'])[0]['steps']
    self.assertIn(dict(op='require_can_activate',ref='source',value=0),s)
    self.assertIn(dict(op='activate',ref='source',value=0),s)
    self.assertIn(dict(op='assert_card',ref='recipient',property='damage',value=3),s)
   s=self.shield('Oriss, Samite Guardian',[f'source_removed_{owner}'])[0]['steps']
   self.assertIn(dict(op='assert_card',ref='source',property='in_play',value=0),s)
   self.assertTrue(all(x['value']==0 for x in s if x.get('ref')=='recipient' and x.get('property')=='damage'))
   self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='hand',index=self.rows['Oriss, Samite Guardian']['csvid'],value=1),s)

 def test_oriss_expiry_has_prior_positive_packet_and_then_real_unprevented_damage(self):
  for owner in (0,1):
   s=self.shield('Oriss, Samite Guardian',[f'cleanup_{owner}'])[0]['steps'];cut=next(i for i,x in enumerate(s) if x['op']=='cleanup')
   self.assertIn(dict(op='cast',ref='bolt',value=1),s[:cut])
   self.assertIn(dict(op='assert_card',ref='recipient',property='damage',value=0),s[:cut])
   self.assertIn(dict(op='cast',ref='second_bolt',value=1),s[cut:])
   self.assertIn(dict(op='assert_card',ref='recipient',property='damage',value=3),s[cut:])


 def test_lymph_absorb_covers_both_controllers_and_leaves_non_slivers(self):
  for owner in (0,1):
   for v,ref in (('effect','friendly'),('opponent_creature','enemy')):
    c=self.replacement('Lymph Sliver',[f'{v}_{owner}'])[0];s=c['steps']
    self.assertEqual(next(x['name'] for x in c['setup'] if x['ref']==ref),'Brood Sliver')
    self.assertIn(dict(op='assert_card',ref=ref,property='damage',value=2),s)
   s=self.replacement('Lymph Sliver',[f'nonmatching_recipient_{owner}'])[0]['steps']
   self.assertIn(dict(op='assert_card',ref='friendly',property='damage',value=3),s)

 def test_temple_pair_controller_type_and_self_exclusion_are_distinct(self):
  for owner in (0,1):
   c=self.replacement('Temple Altisaur',[f'effect_{owner}'])[0];s=c['steps']
   self.assertEqual(next(x['name'] for x in c['setup'] if x['ref']=='friendly'),'Temple Altisaur')
   self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='battlefield',index=self.rows['Temple Altisaur']['csvid'],value=2),s)
   self.assertIn(dict(op='assert_card',ref='friendly',property='damage',value=1),s)
   s=self.replacement('Temple Altisaur',[f'source_damage_{owner}'])[0]['steps']
   self.assertIn(dict(op='assert_card',ref='source',property='damage',value=1),s)
   for v,ref in (('self_exclusion','source'),('nonmatching_recipient','friendly'),('opponent_creature','enemy')):
    s=self.replacement('Temple Altisaur',[f'{v}_{owner}'])[0]['steps']
    self.assertIn(dict(op='assert_card',ref=ref,property='damage',value=3),s)

 def test_spirit_uses_all_five_own_colors_and_colorless_does_not_substitute(self):
  for owner in (0,1):
   s=self.replacement('Spirit of Resistance',[f'effect_{owner}'])[0]['steps']
   self.assertTrue(all(x['value']==20 for x in s if x['op']=='assert_life' and x['player']==owner))
   c=self.replacement('Spirit of Resistance',[f'without_condition_{owner}'])[0]
   self.assertEqual(next(x['name'] for x in c['setup'] if x['ref']=='black_color'),'Sol Ring')
   self.assertIn(dict(op='assert_life',player=owner,value=17),c['steps'])
   c=self.replacement('Spirit of Resistance',[f'opponent_condition_{owner}'])[0]
   self.assertEqual(next(x['player'] for x in c['setup'] if x['ref']=='black_color'),1-owner)
   self.assertIn(dict(op='assert_life',player=owner,value=17),c['steps'])

 def test_bronze_horse_positive_and_wrong_controller_ally_controls(self):
  for owner in (0,1):
   s=self.replacement('Bronze Horse',[f'effect_{owner}'])[0]['steps']
   self.assertIn(dict(op='choose_target',player=1-owner,ref='source'),s)
   self.assertTrue(all(x['value']==0 for x in s if x.get('property')=='damage' and x.get('ref')=='source'))
   c=self.replacement('Bronze Horse',[f'without_condition_{owner}'])[0]
   self.assertEqual(next(x['player'] for x in c['setup'] if x['ref']=='friendly'),1-owner)
   self.assertIn(dict(op='assert_card',ref='source',property='damage',value=3),c['steps'])
   s=self.replacement('Bronze Horse',[f'nonmatching_recipient_{owner}'])[0]['steps']
   self.assertIn(dict(op='assert_card',ref='friendly',property='damage',value=3),s)

 def test_condition_removal_is_after_real_matching_positive(self):
  for owner in (0,1):
   s=self.replacement('Bronze Horse',[f'ally_removed_{owner}'])[0]['steps'];cut=next(i for i,x in enumerate(s) if x.get('op')=='cast' and x.get('ref')=='bounce')
   self.assertIn(dict(op='assert_card',ref='source',property='damage',value=0),s[:cut])
   self.assertIn(dict(op='assert_card',ref='source',property='damage',value=3),s[cut:])
   s=self.replacement('Spirit of Resistance',[f'condition_removed_{owner}'])[0]['steps'];cut=next(i for i,x in enumerate(s) if x.get('op')=='cast' and x.get('ref')=='bounce')
   self.assertIn(dict(op='assert_life',player=owner,value=20),s[:cut])
   self.assertIn(dict(op='assert_life',player=owner,value=17),s[cut:])
   self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='hand',index=self.rows['Scathe Zombies']['csvid'],value=1),s)

 def test_modifier_actual_creature_ability_distinguishes_spell_only_prevention(self):
  for owner in (0,1):
   for name in ('Plated Pegasus','Purity','Gisela, Blade of Goldnight','Bronze Horse'):
    s=self.replacement(name,[f'creature_ability_{owner}'])[0]['steps']
    self.assertIn(dict(op='require_can_activate',ref='ping_source',value=1),s)
    self.assertIn(dict(op='activate',ref='ping_source',value=1),s)
    self.assertIn(dict(op='assert_card',ref='ping_source',property='tapped',value=1),s)
   s=self.replacement('Plated Pegasus',[f'creature_ability_{owner}'])[0]['steps']
   self.assertIn(dict(op='assert_card',ref='friendly',property='damage',value=1),s)
   s=self.replacement('Purity',[f'creature_ability_{owner}'])[0]['steps']
   self.assertIn(dict(op='assert_life',player=owner,value=21),s)
   s=self.replacement('Gisela, Blade of Goldnight',[f'creature_ability_{owner}'])[0]['steps']
   self.assertTrue(all(x['value']==0 for x in s if x.get('property')=='damage' and x.get('ref')=='friendly'))
   s=self.replacement('Bronze Horse',[f'creature_ability_{owner}'])[0]['steps']
   self.assertIn(dict(op='assert_card',ref='source',property='damage',value=1),s)

 def test_modifier_untargeted_spell_is_real_and_bronze_cannot_prevent_it(self):
  for owner in (0,1):
   s=self.replacement('Bronze Horse',[f'untargeted_spell_{owner}'])[0]['steps']
   self.assertIn(dict(op='cast',ref='sweep',value=1),s)
   self.assertFalse(any(x['op'] in ('choose_target','choose_player') for x in s))
   self.assertIn(dict(op='assert_card',ref='source',property='damage',value=2),s)
   s=self.replacement('Plated Pegasus',[f'untargeted_spell_{owner}'])[0]['steps']
   self.assertIn(dict(op='assert_card',ref='friendly',property='damage',value=1),s)
   s=self.replacement('Vigor',[f'untargeted_spell_{owner}'])[0]['steps']
   self.assertIn(dict(op='assert_card',ref='friendly',property='plus1_counters',value=2),s)
   self.assertIn(dict(op='assert_card',ref='source',property='damage',value=2),s)

 def test_vigor_large_combat_proves_assignment_before_counter_growth(self):
  for owner in (0,1):
   c=self.replacement('Vigor',[f'large_combat_{owner}'])[0];s=c['steps']
   self.assertEqual(next(x['name'] for x in c['setup'] if x['ref']=='attacker'),'Krosan Colossus')
   self.assertIn(dict(op='assert_card',ref='friendly',property='plus1_counters',value=9),s)
   self.assertIn(dict(op='assert_card',ref='friendly',property='power',value=16),s)
   self.assertIn(dict(op='assert_card',ref='attacker',property='in_play',value=1),s)
   self.assertIn(dict(op='assert_card',ref='attacker',property='damage',value=7),s)
   self.assertIn('Krosan Colossus Morph from face-down state',c['_recipe']['untested_components'])


 def test_bronze_opponent_creature_really_targets_the_opposing_control(self):
  for owner in (0,1):
   s=self.replacement('Bronze Horse',[f'opponent_creature_{owner}'])[0]['steps']
   self.assertIn(dict(op='choose_target',player=1-owner,ref='enemy'),s)
   self.assertIn(dict(op='assert_card',ref='enemy',property='damage',value=3),s)
   self.assertNotIn(dict(op='choose_target',player=1-owner,ref='source'),s)

 def test_horde_real_entry_counts_other_own_battlefield_and_grave_zombies(self):
  for owner in (0,1):
   c=self.replacement('Unbreathing Horde',[f'effect_{owner}'])[0];s=c['steps']
   count=lambda pl,zone:sum(x['name']=='Scathe Zombies' and x['player']==pl and x['zone']==zone for x in c['setup'])
   self.assertEqual((count(owner,'battlefield'),count(owner,'graveyard'),count(1-owner,'battlefield'),count(1-owner,'graveyard')),(1,2,1,1))
   self.assertIn(dict(op='assert_card',ref='source',property='plus1_counters',value=3),s)
   self.assertIn(dict(op='assert_card',ref='source',property='plus1_counters',value=2),s)
   for v,n in (('without_battlefield_zombie',2),('without_grave_zombies',1)):
    s=self.replacement('Unbreathing Horde',[f'{v}_{owner}'])[0]['steps']
    self.assertIn(dict(op='assert_card',ref='source',property='plus1_counters',value=n),s)
   self.assertIn(dict(op='cast',ref='growth',value=1),s)
   self.assertIn(dict(op='assert_card',ref='source',property='toughness',value=3),s)

 def test_horde_with_zero_counters_still_prevents_using_real_growth(self):
  for owner in (0,1):
   s=self.replacement('Unbreathing Horde',[f'zero_counters_{owner}'])[0]['steps']
   self.assertEqual(sum(x['op']=='cast' and x.get('ref','').startswith('packet_') for x in s),4)
   self.assertIn(dict(op='assert_card',ref='source',property='plus1_counters',value=0),s)
   self.assertIn(dict(op='require_card',ref='source',property='in_play',value=1),s)
   self.assertTrue(all(x['value']==0 for x in s if x.get('ref')=='source' and x.get('property')=='damage'))

 def test_champion_real_landfall_counter_and_tapped_land_receipts(self):
  for owner in (0,1):
   s=self.replacement('Undergrowth Champion',[f'effect_{owner}'])[0]['steps']
   self.assertIn(dict(op='cast',ref='land_search',value=1),s)
   self.assertIn(dict(op='choose_zone_card',player=owner,value=self.rows['Forest']['csvid']),s)
   self.assertIn(dict(op='assert_card',ref='new_land',property='tapped',value=1),s)
   self.assertIn(dict(op='assert_card',ref='source',property='plus1_counters',value=1),s)
   self.assertIn(dict(op='assert_card',ref='source',property='plus1_counters',value=0),s)
   self.assertIn(dict(op='assert_zone',player=owner,zone='library',value=6),s)
   self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='library',index=self.rows['Forest']['csvid'],value=1),s)
   self.assertFalse(any(x['op']=='assert_zone_card' and x['player']==owner and x['zone']=='library' for x in s))

 def test_champion_counter_exhaustion_really_allows_next_damage(self):
  for owner in (0,1):
   s=self.replacement('Undergrowth Champion',[f'repeat_damage_{owner}'])[0]['steps'];cut=s.index(dict(op='cast',ref='packet_1',value=1))
   self.assertIn(dict(op='require_card',ref='source',property='in_play',value=1),s[:cut])
   self.assertIn(dict(op='assert_card',ref='source',property='plus1_counters',value=0),s[:cut])
   self.assertIn(dict(op='assert_card',ref='source',property='in_play',value=0),s[cut:])
   self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='graveyard',index=self.rows['Undergrowth Champion']['csvid'],value=1),s)

 def test_champion_opponent_land_nonland_fail_search_and_no_counter_controls(self):
  for owner in (0,1):
   for v in ('opponent_land','nonland_spell','failed_search','without_counter'):
    s=self.replacement('Undergrowth Champion',[f'{v}_{owner}'])[0]['steps']
    self.assertTrue(all(x['value']==0 for x in s if x.get('property')=='plus1_counters' and x.get('ref')=='source'))
    self.assertIn(dict(op='assert_card',ref='source',property='in_play',value=0),s)
   s=self.replacement('Undergrowth Champion',[f'opponent_land_{owner}'])[0]['steps']
   self.assertIn(dict(op='choose_zone_card',player=1-owner,value=self.rows['Forest']['csvid']),s)
   s=self.replacement('Undergrowth Champion',[f'failed_search_{owner}'])[0]['steps']
   self.assertIn(dict(op='choose_zone',player=-1,value=-1),s)
   s=self.replacement('Undergrowth Champion',[f'nonland_spell_{owner}'])[0]['steps']
   self.assertIn(dict(op='cast',ref='nonland',value=1),s)
   self.assertIn(dict(op='assert_card',ref='control',property='power',value=10),s)

 def test_gloom_combat_exiles_its_own_deck_and_real_outgoing_damage(self):
  for owner in (0,1):
   s=self.replacement('Gloom Surgeon',[f'effect_{owner}'])[0]['steps']
   self.assertIn(dict(op='block',ref='source',other='attacker',value=1),s)
   self.assertIn(dict(op='assert_zone',player=owner,zone='library',value=4),s)
   self.assertIn(dict(op='assert_zone',player=owner,zone='exile',value=5),s)
   self.assertIn(dict(op='assert_zone',player=1-owner,zone='library',value=7),s)
   self.assertIn(dict(op='assert_card',ref='attacker',property='damage',value=2),s)

 def test_gloom_short_or_empty_deck_never_limits_full_prevention(self):
  for owner in (0,1):
   for v,exiled in (('short_library',1),('bare_zones',0)):
    c=self.replacement('Gloom Surgeon',[f'{v}_{owner}'])[0];s=c['steps']
    self.assertEqual(c['fixture'],'empty_main')
    self.assertIn(dict(op='require_card',ref='source',property='in_play',value=1),s)
    self.assertTrue(all(x['value']==0 for x in s if x.get('ref')=='source' and x.get('property')=='damage'))
    self.assertIn(dict(op='assert_zone',player=owner,zone='exile',value=exiled),s)

 def test_gloom_noncombat_source_dies_without_library_exile_for_both_casters(self):
  for owner in (0,1):
   for v,caster in (('noncombat',1-owner),('friendly_source',owner)):
    s=self.replacement('Gloom Surgeon',[f'{v}_{owner}'])[0]['steps']
    self.assertIn(dict(op='choose_target',player=caster,ref='source'),s)
    self.assertIn(dict(op='assert_card',ref='source',property='in_play',value=0),s)
    self.assertIn(dict(op='assert_zone',player=owner,zone='library',value=7),s)
    self.assertIn(dict(op='assert_zone',player=owner,zone='exile',value=2),s)


 def test_entry_shield_pins_and_effects_fail_closed(self):
  name='Selfless Squire';profile=_shield_pins()['sources'][name]
  d=dict(family='prevention',version=2,params=copy.deepcopy(profile['params']),oracle_text=self.oracle[name]['oracle_text'],variants=['effect_0'])
  for field,value in (('oracle_text','Prevent all damage.'),('power','2'),('mana_cost','{W}'),('layout','token')):
   with self.assertRaises(ValueError):spec(d,dict(self.oracle[name],**{field:value}))
  bad=copy.deepcopy(d);bad['params']['effect']='artifact_creatures'
  with self.assertRaises(ValueError):spec(bad,self.oracle[name])
  for vs in ([],['unknown_0'],['effect_0','effect_0']):
   bad=dict(d,variants=vs)
   with self.assertRaises(ValueError):expand(name,self.rows[name]['csvid'],bad,spec(bad,self.oracle[name]),vs,self.rows,self.oracle)

 def test_entry_shield_pending_source_is_physically_in_hand_until_resolution(self):
  for name in ('Ethersworn Shieldmage','Selfless Squire','Haze Frog'):
   for owner in (0,1):
    s=self.replacement(name,[f'pending_snapshot_{owner}'])[0]['steps'];cut=next(i for i,x in enumerate(s) if x['op']=='capture')
    self.assertIn(dict(op='cast',ref='source',value=1),s[:cut])
    self.assertIn(dict(op='assert_stack',value=1),s[:cut])
    self.assertIn(dict(op='assert_zone',player=owner,zone='hand',value=5),s[:cut])
    self.assertIn(dict(op='resolve'),s[cut:])

 def test_entry_shield_real_response_stack_and_payment_for_both_owners(self):
  for name in ('Ethersworn Shieldmage','Selfless Squire','Haze Frog'):
   for owner in (0,1):
    s=self.replacement(name,[f'response_cast_{owner}'])[0]['steps']
    self.assertLess(s.index(dict(op='cast',ref='packet_0',value=1)),s.index(dict(op='cast_response',ref='source',value=1)))
    self.assertIn(dict(op='assert_stack',value=2),s)
    self.assertIn(dict(op='resolve_response'),s)
    self.assertIn(dict(op='assert_stack',value=0),s)
    self.assertFalse(any(x['op']=='cast' and x.get('ref')=='source' for x in s))

 def test_shieldmage_current_artifact_creatures_both_owners_and_nonartifact_control(self):
  for owner in (0,1):
   for v,ref in (('effect','artifact'),('opponent_creature','enemy_artifact'),('source_damage','source')):
    s=self.replacement('Ethersworn Shieldmage',[f'{v}_{owner}'])[0]['steps']
    self.assertTrue(all(x['value']==0 for x in s if x.get('ref')==ref and x.get('property')=='damage'))
   s=self.replacement('Ethersworn Shieldmage',[f'nonartifact_{owner}'])[0]['steps']
   self.assertIn(dict(op='assert_card',ref='ordinary',property='damage',value=3),s)

 def test_shieldmage_late_real_artifact_cast_is_in_the_turn_shield(self):
  for owner in (0,1):
   c=self.replacement('Ethersworn Shieldmage',[f'late_artifact_{owner}'])[0];s=c['steps']
   self.assertEqual(next(x['name'] for x in c['setup'] if x['ref']=='late'),'Brass Man')
   self.assertLess(s.index(dict(op='cast',ref='source',value=1)),s.index(dict(op='cast',ref='late',value=1)))
   self.assertLess(s.index(dict(op='cast',ref='late',value=1)),s.index(dict(op='cast',ref='packet_0',value=1)))
   self.assertIn(dict(op='choose_target',player=1-owner,ref='late'),s)
   self.assertIn(dict(op='require_card',ref='late',property='in_play',value=1),s)
   self.assertTrue(all(x['value']==0 for x in s if x.get('ref')=='late' and x.get('property')=='damage'))

 def test_shieldmage_real_type_change_is_before_the_damage_packet(self):
  for owner in (0,1):
   s=self.replacement('Ethersworn Shieldmage',[f'type_changed_{owner}'])[0]['steps']
   self.assertIn(dict(op='require_can_activate',ref='coating',value=1),s)
   self.assertIn(dict(op='choose_target',player=owner,ref='ordinary'),s)
   self.assertIn(dict(op='activate',ref='coating',value=1),s)
   self.assertIn(dict(op='assert_matching_permanents',player=owner,index=self.rows['Scaled Wurm']['csvid'],property='artifact',value=1),s)
   self.assertIn(dict(op='assert_card',ref='coating',property='tapped',value=1),s)
   self.assertTrue(all(x['value']==0 for x in s if x.get('ref')=='ordinary' and x.get('property')=='damage'))

 def test_selfless_squire_mandatory_counter_quantity_and_caster_independence(self):
  for owner in (0,1):
   for v,n in (('effect',3),('small_damage',2),('repeat_damage',6),('friendly_source',3),('creature_damage',10)):
    s=self.replacement('Selfless Squire',[f'{v}_{owner}'])[0]['steps']
    self.assertIn(dict(op='assert_card',ref='source',property='plus1_counters',value=n),s)
    self.assertIn(dict(op='assert_card',ref='source',property='power',value=1+n),s)
    self.assertIn(dict(op='assert_life',player=owner,value=20),s)
   s=self.replacement('Selfless Squire',[f'opponent_player_{owner}'])[0]['steps']
   self.assertIn(dict(op='assert_life',player=1-owner,value=17),s)
   self.assertTrue(all(x['value']==0 for x in s if x.get('ref')=='source' and x.get('property')=='plus1_counters'))

 def test_selfless_squire_external_prevention_after_etb_shield_expiry(self):
  for owner in (0,1):
   s=self.replacement('Selfless Squire',[f'external_prevention_{owner}'])[0]['steps']
   self.assertIn(dict(op='assert_card',ref='source',property='plus1_counters',value=3),s)
   self.assertIn(dict(op='cast',ref='external',value=1),s)
   self.assertIn(dict(op='assert_card',ref='source',property='plus1_counters',value=13),s)
   self.assertEqual([x['player'] for x in s if x['op']=='untap_step'],[owner,1-owner])
   last=max(i for i,x in enumerate(s) if x['op']=='untap_step')
   self.assertIn(dict(op='turn',player=1-owner,value=0),s[last+1:])

 def test_haze_frog_own_real_outgoing_damage_and_others_prevented(self):
  for owner in (0,1):
   s=self.replacement('Haze Frog',[f'effect_{owner}'])[0]['steps']
   self.assertIn(dict(op='block',ref='source',other='attacker',value=1),s)
   self.assertIn(dict(op='assert_card',ref='attacker',property='damage',value=2),s)
   self.assertTrue(all(x['value']==0 for x in s if x.get('ref')=='source' and x.get('property')=='damage'))
   self.assertIn(dict(op='assert_life',player=owner,value=20),s)
   s=self.replacement('Haze Frog',[f'noncombat_{owner}'])[0]['steps']
   self.assertIn(dict(op='assert_card',ref='ordinary',property='damage',value=3),s)

 def test_entry_shields_source_removal_and_expiry_have_prior_positive_episode(self):
  for owner in (0,1):
   s=self.replacement('Selfless Squire',[f'source_removed_{owner}'])[0]['steps'];cut=s.index(dict(op='cast',ref='bounce',value=1))
   self.assertIn(dict(op='assert_card',ref='source',property='plus1_counters',value=3),s[:cut])
   self.assertIn(dict(op='assert_card',ref='source',property='in_play',value=0),s[cut:])
   self.assertTrue(all(x['value']==20 for x in s if x['op']=='assert_life' and x['player']==owner))
   s=self.replacement('Ethersworn Shieldmage',[f'cleanup_{owner}'])[0]['steps'];cut=next(i for i,x in enumerate(s) if x['op']=='cleanup')
   self.assertIn(dict(op='assert_card',ref='artifact',property='damage',value=0),s[:cut])
   self.assertIn(dict(op='assert_card',ref='artifact',property='in_play',value=0),s[cut:])
   s=self.replacement('Haze Frog',[f'source_removed_{owner}'])[0]['steps']
   self.assertIn(dict(op='cast',ref='extra_combat',value=1),s)
   self.assertIn(dict(op='block',ref='ordinary',other='attacker',value=1),s)
   self.assertTrue(all(x['value']==0 for x in s if x.get('ref')=='ordinary' and x.get('property')=='damage'))


 def test_entry_shield_steps_use_production_generator_ops_and_observations(self):
  from generate_cases import OPS,PROPERTIES,TOKEN_PROPERTIES,REF_OPS,validate_untap_step
  registered={r['csvid'] for r in self.rows.values()}
  for name in ('Ethersworn Shieldmage','Selfless Squire','Haze Frog'):
   profile=_shield_pins()['sources'][name];d=dict(family='prevention',version=2,params=copy.deepcopy(profile['params']),oracle_text=self.oracle[name]['oracle_text'],variants=['effect_0'])
   p=spec(d,self.oracle[name]);d['variants']=variants(p)
   for c in self.cases(name,d=d):
    refs={x['ref'] for x in c['setup']}
    for x in c['steps']:
     self.assertIn(x['op'],OPS)
     self.assertIn(x.get('property','power'),PROPERTIES|(TOKEN_PROPERTIES if x['op']=='assert_matching_permanents' else set()))
     if x['op'] in REF_OPS:self.assertIn(x['ref'],refs)
     if x['op']=='assert_matching_permanents':
      self.assertIn(x['player'],(0,1));self.assertIn(x['index'],registered)
     if x['op']=='untap_step':validate_untap_step(x)


class UnpreventablePreventionRecipesTest(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.oracle=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards']
  cls.rows={r['name']:r for r in registry()}
  cls.pins=json.loads((Path(__file__).parent/'data/prevention_wave7_witnesses.json').read_text(encoding='utf8'))

 def declaration(self,name,selected):
  return dict(family='prevention',version=2,oracle_text=self.oracle[name]['oracle_text'],params=copy.deepcopy(self.pins['sources'][name]['params']),variants=selected)

 def cases(self,name,selected,oracle=None,declaration=None):
  o=self.oracle if oracle is None else oracle;d=declaration or self.declaration(name,selected)
  return expand(name,self.rows[name]['csvid'],d,spec(d,o[name]),d['variants'],self.rows,o)

 def test_whole_source_cost_and_effect_pin_and_mode_fail_closed(self):
  name='Banefire';d=self.declaration(name,['creature_0'])
  for key,value in (('mana_cost','{X}{R}{R}'),('oracle_text',self.oracle[name]['oracle_text']+' Draw a card.'),('layout','transform'),('keywords',['Flying'])):
   with self.subTest(key=key),self.assertRaises(ValueError):
    self.cases(name,d['variants'],oracle=dict(self.oracle,**{name:dict(self.oracle[name],**{key:value})}))
  for key,value in (('mode','global'),('schema',8),('target_repetitions',1),('gaps',[])):
   bad=copy.deepcopy(d);bad['params'][key]=value
   with self.subTest(key=key),self.assertRaises(ValueError):self.cases(name,bad['variants'],declaration=bad)

 def test_shield_source_damage_and_creature_witness_pins_fail_closed(self):
  for nm,key,value in (('Safe Passage','oracle_text','Prevent all damage.'),('Lightning Bolt','mana_cost','{R}{R}'),('Scaled Wurm','toughness','8')):
   with self.subTest(name=nm),self.assertRaises(ValueError):
    self.cases('Flaring Pain',['creature_0'],oracle=dict(self.oracle,**{nm:dict(self.oracle[nm],**{key:value})}))

 def test_schema7_identity_and_numeric_json_types_fail_closed(self):
  for name in ('Banefire','Lava Burst'):
   original=self.declaration(name,['creature_0'])
   for field,value in (('family','damage_spell'),('family',None),('version',2.0),('version',True)):
    bad=copy.deepcopy(original);bad[field]=value
    with self.subTest(name=name,field=field,value=value),self.assertRaises(ValueError):
     spec(bad,self.oracle[name])
   for field,value in (('schema',7.0),('schema',True),('target_repetitions',float(original['params']['target_repetitions'])),('target_repetitions',True),('gaps',tuple(original['params']['gaps']))):
    bad=copy.deepcopy(original);bad['params'][field]=value
    with self.subTest(name=name,field=field,value=value),self.assertRaises(ValueError):
     spec(bad,self.oracle[name])

 def test_every_profile_uses_actual_casting_and_shield_control_for_both_players(self):
  for name in self.pins['sources']:
   for owner in (0,1):
    c=self.cases(name,[f'creature_{owner}'])[0];s=c['steps']
    source=next(i for i,x in enumerate(s) if x['op']=='cast' and x['ref']=='source')
    shield=next(i for i,x in enumerate(s) if x['op']=='cast' and x['ref']=='shield')
    control=next(i for i,x in enumerate(s) if x['op']=='cast' and x['ref']=='control_bolt')
    self.assertLess(shield,control);self.assertLess(control,source)
    self.assertIn(dict(op='assert_card',ref='recipient',property='damage',value=0),s[control:source])
    self.assertIn(dict(op='assert_card',ref='recipient',property='damage',value=5 if name=='Banefire' else 3),s[source:])
    self.assertEqual(next(x['player'] for x in c['setup'] if x['ref']=='source'),owner)
    self.assertEqual(next(x['player'] for x in c['setup'] if x['ref']=='shield'),1-owner)
    self.assertIn('source', [x['ref'] for x in s if x['op']=='cast'])

 def test_banefire_threshold_and_lava_burst_recipient_type_are_separate_rules(self):
  below,at=self.cases('Banefire',['threshold_below_0','threshold_at_0'])
  self.assertIn(dict(op='choose_x',player=0,value=4),below['steps'])
  self.assertNotIn(dict(op='assert_card',ref='recipient',property='damage',value=4),below['steps'])
  self.assertIn(dict(op='assert_card',ref='recipient',property='damage',value=5),at['steps'])
  creature,player=self.cases('Lava Burst',['creature_0','player_0'])
  self.assertIn(dict(op='assert_card',ref='recipient',property='damage',value=3),creature['steps'])
  self.assertIn(dict(op='assert_life',player=1,value=20),player['steps'])
  self.assertNotIn(dict(op='assert_life',player=1,value=17),player['steps'])

 def test_hellbent_hand_control_and_lethal_exile_keep_correct_oracle_expectations(self):
  empty,occupied,lethal=self.cases('Demonfire',['creature_0','with_hand_card_0','lethal_exile_0'])
  self.assertNotIn('hand_control',[x['ref'] for x in empty['setup']])
  self.assertEqual(next(x['name'] for x in occupied['setup'] if x['ref']=='hand_control'),'Forest')
  self.assertIn(dict(op='assert_card',ref='recipient',property='damage',value=3),empty['steps'])
  self.assertNotIn(dict(op='assert_card',ref='recipient',property='damage',value=3),occupied['steps'])
  self.assertIn(dict(op='assert_zone_multiplicity',player=1,zone='exile',index=self.rows['Grizzly Bears']['csvid'],value=1),lethal['steps'])

 def test_global_effect_has_real_no_effect_and_expiration_controls(self):
  no_effect,cleanup=self.cases('Flaring Pain',['without_effect_0','cleanup_0'])
  self.assertNotIn('source',[x['ref'] for x in no_effect['steps'] if x['op']=='cast'])
  self.assertNotIn(dict(op='assert_card',ref='recipient',property='damage',value=3),no_effect['steps'])
  at=next(i for i,x in enumerate(cleanup['steps']) if x['op']=='cleanup')
  self.assertIn(dict(op='assert_card',ref='recipient',property='damage',value=3),cleanup['steps'][:at])
  self.assertIn(dict(op='assert_card',ref='recipient',property='damage',value=0),cleanup['steps'][at:])
  self.assertIn('next_shield',[x['ref'] for x in cleanup['steps'][at:] if x['op']=='cast'])

 def test_pending_snapshots_and_lost_targets_keep_real_response_context(self):
  for name in self.pins['sources']:
   c=self.cases(name,['pending_snapshot_1'])[0];s=c['steps'];at=next(i for i,x in enumerate(s) if x['op']=='capture')
   self.assertTrue(any(x['op']=='cast' and x['ref']=='source' for x in s[:at]))
   self.assertEqual(s[at-1],dict(op='assert_stack',value=1))
   self.assertIn('restore',[x['op'] for x in s]);self.assertIn('assert_recorded_state',[x['op'] for x in s])
  c=self.cases('Lava Burst',['lost_target_0'])[0]
  self.assertIn(dict(op='cast_response',ref='bounce',value=1),c['steps'])
  self.assertIn(dict(op='resolve_response'),c['steps'])
  self.assertIn(dict(op='assert_zone_multiplicity',player=1,zone='hand',index=self.rows['Scaled Wurm']['csvid'],value=1),c['steps'])

 def test_invalid_variants_and_zero_mana_casts_fail_closed(self):
  for name in self.pins['sources']:
   for vs in ([],['bad_0'],['creature_0','creature_0']):
    with self.subTest(name=name,vs=vs),self.assertRaises(ValueError):self.cases(name,vs)
   c=self.cases(name,['insufficient_mana_0'])[0]
   self.assertIn(dict(op='cast',ref='source',value=0),c['steps'])
   self.assertNotIn(dict(op='choose_x',player=0,value=3),c['steps'])


if __name__=='__main__':unittest.main()
