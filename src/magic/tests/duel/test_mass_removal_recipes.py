import copy
import json
import unittest
from pathlib import Path
from generate_cases import registry
from recipes import expand,explicit_spec
from mass_removal_recipes import matches,variants


class MassRemovalRecipesTest(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.oracle=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards'];cls.rows={r['name']:r for r in registry()}

 def declaration(self,name,selected=None):
  configs={
   'Planar Cleansing':dict(scope='nonland_permanent'),
   'Final Judgment':dict(effect='exile'),
   'Upheaval':dict(effect='bounce',scope='permanent'),
   'Apocalypse':dict(effect='exile',scope='permanent',discard_hand=True),
   'Perish':dict(scope='green_creature',no_regenerate=True),
   'Serene Heart':dict(scope='aura'),
   'Tranquil Domain':dict(scope='nonaura_enchantment'),
   "Hurkyl's Recall":dict(effect='bounce',scope='artifact',controller='target_owner'),
   "Kaya's Wrath":dict(gain_per=1,gain_scope='self'),
   'Retribution of the Meek':dict(limit=dict(field='power',op='ge',value=4),no_regenerate=True),
   'Ritual of Soot':dict(limit=dict(field='mana_value',op='le',value=3)),
   "Nevinyrral's Disk":dict(mode='activation',scope='artifact_creature_enchantment',mana='{1}',tap=True,enters_tapped=True),
   'Magus of the Disk':dict(mode='activation',scope='artifact_creature_enchantment',mana='{1}',tap=True,enters_tapped=True),
   'Perilous Vault':dict(mode='activation',effect='exile',scope='nonland_permanent',mana='{5}',tap=True,self='exile'),
   'Mageta the Lion':dict(mode='activation',mana='{2}{W}{W}',tap=True,discard=2,exclude_self=True,no_regenerate=True),
   'Tranquil Grove':dict(mode='activation',scope='enchantment',mana='{1}{G}{G}',exclude_self=True),
   'Aetherize':dict(effect='bounce',state='attacking'),
   "Multani's Decree":dict(scope='enchantment',gain_per=2),
   'Decree of Pain':dict(no_regenerate=True,draw_per=1),
   'Supreme Verdict':dict(uncounterable=True),
   'Overwhelming Forces':dict(controller='target_opponent',draw_per=1),
  }
  p=dict(mode='spell',effect='destroy',scope='creature',controller='all',state='any',limit=None,exclude_self=False,no_regenerate=False,gain_per=0,gain_scope='all',draw_per=0,discard_hand=False,cost=dict(mana='',tap=False,self='none',discard=0,pay_life=0),keywords=[],enters_tapped=False,uncounterable=False)
  for k,v in configs[name].items():
   if k in p['cost']:p['cost'][k]=v
   else:p[k]=v
  return dict(family='mass_removal',version=2,params=p,oracle_text=self.oracle[name]['oracle_text'],variants=selected or ['mixed_0'])

 def cases(self,name,selected=None,d=None,o=None):
  return expand(name,self.rows[name]['csvid'],d or self.declaration(name,selected),self.oracle[name],self.rows,oracle_context=o or self.oracle)

 def has(self,steps,**fields):return any(all(s.get(k)==v for k,v in fields.items()) for s in steps)

 def copies(self,steps,n,player,zone,count):
  return self.has(steps,op='assert_zone_multiplicity',player=player,zone=zone,index=self.rows[n]['csvid'],value=count)

 def test_scope_controller_and_receipt_pins_reject_changed_whole_effect(self):
  for patch in (dict(scope='permanent'),dict(controller='self'),dict(gain_per=1),dict(effect='exile')):
   d=self.declaration('Planar Cleansing');d['params'].update(patch)
   with self.assertRaises(ValueError):self.cases('Planar Cleansing',d=d)

 def test_characteristic_limits_reject_boolean_or_unreviewed_fields(self):
  for b in (dict(field='power',op='ge',value=True),dict(field='power',op='gt',value=4),dict(field='cmc',op='le',value=3)):
   d=self.declaration('Ritual of Soot');d['params']['limit']=b
   with self.assertRaises(ValueError):self.cases('Ritual of Soot',d=d)

 def test_intersection_witnesses_match_each_relevant_type_once(self):
  p=explicit_spec(self.declaration('Planar Cleansing'),self.oracle['Planar Cleansing'])
  for name,want in (('Dryad Arbor',False),('Darksteel Citadel',False),('Nyxborn Shieldmate',True),('Whip of Erebos',True)):
   self.assertEqual(matches(self.oracle[name],p),want)

 def test_destroy_spares_indestructible_but_exile_and_bounce_do_not(self):
  for n in ('Darksteel Myr','Darksteel Gargoyle'):
   self.assertTrue(self.copies(self.cases('Planar Cleansing')[0]['steps'],n,0,'battlefield',1))
   self.assertTrue(self.copies(self.cases('Final Judgment')[0]['steps'],n,0,'exile',1))
   self.assertTrue(self.copies(self.cases('Upheaval')[0]['steps'],n,0,'hand',1))

 def test_owner_scoped_bounce_distinguishes_stolen_owner_from_controller(self):
  s=self.cases("Hurkyl's Recall",['stolen_0'])[0]['steps']
  for pl,battle,hand in ((0,0,0),(1,1,1)):
   self.assertTrue(self.copies(s,'Sol Ring',pl,'battlefield',battle));self.assertTrue(self.copies(s,'Sol Ring',pl,'hand',hand))

 def test_exiling_creature_sends_its_nonexiled_aura_to_graveyard(self):
  s=self.cases('Final Judgment',['aura_0'])[0]['steps']
  for pl in (0,1):
   self.assertTrue(self.copies(s,'Pacifism',pl,'graveyard',1));self.assertTrue(self.copies(s,'Pacifism',pl,'exile',0))

 def test_aura_and_non_aura_mass_filters_are_distinct(self):
  aura=self.cases('Serene Heart',['aura_0'])[0]['steps'];plain=self.cases('Tranquil Domain',['aura_0'])[0]['steps']
  self.assertTrue(self.copies(aura,'Pacifism',0,'graveyard',1));self.assertTrue(self.copies(aura,'Seal of Cleansing',0,'battlefield',1))
  self.assertTrue(self.copies(plain,'Pacifism',0,'battlefield',1));self.assertTrue(self.copies(plain,'Seal of Cleansing',0,'graveyard',1))

 def test_token_copies_do_not_accumulate_in_nonbattlefield_zones(self):
  s=self.cases('Final Judgment',['tokens_0'])[0]['steps']
  self.assertTrue(self.copies(s,'Grizzly Bears',0,'exile',1))
  s=self.cases('Upheaval',['tokens_0'])[0]['steps'];self.assertTrue(self.copies(s,'Grizzly Bears',0,'hand',2))

 def test_apocalypse_discards_hand_after_exiling_permanents(self):
  s=self.cases('Apocalypse')[0]['steps'];self.assertTrue(self.has(s,op='assert_zone',player=0,zone='hand',value=0))
  self.assertTrue(self.copies(s,'Giant Growth',0,'graveyard',2));self.assertTrue(self.copies(s,'Sol Ring',0,'exile',1))
  self.assertTrue(self.has(s,op='assert_zone',player=1,zone='hand',value=4))

 def test_activation_self_exile_is_paid_before_mass_effect(self):
  s=self.cases('Perilous Vault')[0]['steps'];resolve=next(i for i,x in enumerate(s) if x['op']=='resolve')
  self.assertTrue(self.has(s[:resolve],op='assert_zone',player=0,zone='exile',value=3))
  self.assertTrue(self.has(s[:resolve],op='assert_stack',value=1));self.assertTrue(self.has(s[:resolve],op='assert_mana',player=0,color=0,value=15))
  self.assertTrue(self.copies(s,'Perilous Vault',0,'exile',1));self.assertTrue(self.copies(s,'Perilous Vault',0,'graveyard',0))

 def test_fresh_disk_stays_tapped_and_cannot_activate(self):
  for n in ("Nevinyrral's Disk",'Magus of the Disk'):
   s=self.cases(n,['fresh_activation_0'])[0]['steps']
   self.assertTrue(self.has(s,op='require_can_activate',ref='source',value=0));self.assertTrue(self.has(s,op='assert_card',ref='source',property='tapped',value=1))
  source=next(c for c in self.cases('Magus of the Disk',['sick_0'])[0]['setup'] if c['ref']=='source')
  self.assertEqual(source['state'],['untapped','sick'])

 def test_paid_discard_costs_do_not_borrow_from_resolution(self):
  s=self.cases('Mageta the Lion')[0]['steps'];resolve=next(i for i,x in enumerate(s) if x['op']=='resolve')
  self.assertTrue(self.has(s[:resolve],op='assert_zone',player=0,zone='hand',value=2));self.assertTrue(self.has(s[:resolve],op='assert_zone',player=0,zone='graveyard',value=6))
  s=self.cases('Mageta the Lion',['empty_hand_cost_1'])[0]['steps'];self.assertTrue(self.has(s,op='require_can_activate',ref='source',value=0))

 def test_two_pending_activations_pay_twice_then_resolve_independently(self):
  s=self.cases('Tranquil Grove',['stacked_activations_0'])[0]['steps']
  self.assertEqual([x['value'] for x in s if x['op']=='assert_stack'],[1,2,0])
  self.assertTrue(self.has(s,op='assert_mana',player=0,color=3,value=16));self.assertTrue(self.copies(s,'Tranquil Grove',0,'battlefield',1))

 def test_regeneration_uses_an_affected_green_creature_for_perish(self):
  case=self.cases('Perish',['regeneration_0'])[0];planned=next(s['ref'] for s in case['steps'] if s['op']=='choose_regeneration');setup=next(c for c in case['setup'] if c['ref']==planned)
  self.assertEqual(setup['name'],'Horned Troll');self.assertTrue(self.has(case['steps'],op='assert_unused_choices',value=1))

 def test_real_attack_declarations_preserve_unselected_creatures(self):
  s=self.cases('Aetherize')[0]['steps'];self.assertTrue(self.has(s,op='begin_attackers'));self.assertTrue(self.has(s,op='attack'));self.assertTrue(self.has(s,op='advance_combat'))
  self.assertTrue(self.copies(s,'Wall of Stone',1,'battlefield',1))

 def test_countered_mass_spell_keeps_board_and_its_derived_receipts(self):
  s=self.cases("Kaya's Wrath",['countered_0'])[0]['steps'];self.assertTrue(self.has(s,op='assert_life',player=0,value=20));self.assertTrue(self.copies(s,'Grizzly Bears',0,'battlefield',1));self.assertTrue(self.copies(s,"Kaya's Wrath",0,'graveyard',1))

 def test_ready_pending_and_resolved_snapshots_replay_all_receipts(self):
  for v in ('ready_snapshot_1','pending_snapshot_1','resolved_snapshot_1'):
   s=self.cases('Decree of Pain',[v])[0]['steps']
   for op in ('capture','restore','ai_roundtrip','record_state','assert_recorded_state'):self.assertTrue(self.has(s,op=op))

 def test_changed_witness_requires_re_review(self):
  o=copy.deepcopy(self.oracle);o['Darksteel Myr']['keywords']=[]
  with self.assertRaises(ValueError):self.cases('Final Judgment',o=o)

 def test_uncounterable_spell_resolves_after_counterspell_is_spent(self):
  s=self.cases('Supreme Verdict',['countered_0'])[0]['steps']
  self.assertTrue(self.has(s,op='cast',ref='source',value=1))
  self.assertTrue(self.copies(s,'Grizzly Bears',0,'battlefield',0))
  self.assertTrue(self.copies(s,'Counterspell',0,'graveyard',1))
  d=self.declaration('Final Judgment');d['params']['uncounterable']=True
  with self.assertRaises(ValueError):self.cases('Final Judgment',d=d)

 def test_opponent_only_target_never_uses_a_positive_self_target(self):
  s=self.cases('Overwhelming Forces',['own_target_0'])[0]['steps']
  self.assertTrue(self.has(s,op='cast',ref='shield',value=1))
  self.assertTrue(self.has(s,op='can_cast',ref='source',value=0))
  self.assertTrue(self.has(s,op='cast',ref='source',value=0))
  self.assertFalse(self.has(s,op='choose_player'))
  self.assertTrue(self.copies(s,'Grizzly Bears',0,'battlefield',1))


class Wave6MassRemovalRecipesTest(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  import removal_recipes,mass_removal_recipes
  cls.module=mass_removal_recipes;cls.pins=removal_recipes.WAVE6_PINS;cls.oracle=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards'];cls.rows={r['name']:r for r in registry()}
 def declaration(self,name='Forced March',selected=None):return dict(family='mass_removal',version=2,params=copy.deepcopy(self.pins['profiles'][name]['params']),oracle_text=self.oracle[name]['oracle_text'],variants=selected or ['effect_0'])
 def cases(self,name='Forced March',selected=None,d=None):return expand(name,self.rows[name]['csvid'],d or self.declaration(name,selected),self.oracle[name],self.rows,self.oracle)
 def has(self,s,**fields):return any(all(x.get(k)==v for k,v in fields.items()) for x in s)
 def test_wave6_whole_mass_predicate_and_cost_pin_rejected(self):
  for patch in ({'scope':'permanent'},{'secondary':{}},{'cost':{}},{'mode':'target'}):
   d=self.declaration('Fracturing Gust');d['params'].update(patch)
   with self.assertRaises(ValueError):self.cases('Fracturing Gust',d=d)
 def test_wave6_x_has_real_choice_and_colored_exact_payment(self):
  for pl in (0,1):
   s=self.cases(selected=[f'payment_x3_{pl}'])[0]['steps'];self.assertTrue(self.has(s,op='choose_x',player=pl,value=3));self.assertTrue(self.has(s,op='set_mana',player=pl,color=0,value=3));self.assertTrue(self.has(s,op='set_mana',player=pl,color=1,value=3))
   for color in range(6):self.assertTrue(self.has(s,op='assert_mana',player=pl,color=color,value=0))
 def test_wave6_zero_x_hits_zero_value_creatures_but_not_lands(self):
  s=self.cases(selected=['effect_x0_0'])[0]['steps']
  self.assertTrue(self.has(s,op='assert_zone_multiplicity',player=1,zone='graveyard',index=self.rows['Memnite']['csvid'],value=1))
  self.assertTrue(self.has(s,op='assert_zone_multiplicity',player=1,zone='battlefield',index=self.rows['Forest']['csvid'],value=1))
 def test_wave6_hybrid_both_routes_use_distinct_exact_pools(self):
  for v,c in [('payment_1',3),('payment_other_hybrid_1',5)]:
   s=self.cases('Fracturing Gust',[v])[0]['steps'];self.assertTrue(self.has(s,op='set_mana',player=1,color=c,value=3));self.assertTrue(self.has(s,op='set_mana',player=1,color=0,value=2))
   for color in range(6):self.assertTrue(self.has(s,op='assert_mana',player=1,color=color,value=0))
 def test_wave6_artifact_enchantment_overlap_counts_once_and_indestructible_zero(self):
  c=self.cases('Fracturing Gust')[0];s=c['steps'];destroyed=[x for x in c['setup'] if x['zone']=='battlefield' and ('Artifact' in self.oracle[x['name']]['type_line'] or 'Enchantment' in self.oracle[x['name']]['type_line']) and 'Indestructible' not in self.oracle[x['name']].get('keywords',[])]
  self.assertTrue(self.has(s,op='assert_life',player=0,value=20+2*len(destroyed)))
  self.assertTrue(self.has(s,op='assert_zone_multiplicity',player=0,zone='battlefield',index=self.rows['Darksteel Relic']['csvid'],value=1))
 def test_wave6_stolen_mass_cards_go_to_original_owner(self):
  s=self.cases(selected=['stolen_0'])[0]['steps'];self.assertTrue(self.has(s,op='assert_zone_multiplicity',player=1,zone='graveyard',index=self.rows['Memnite']['csvid'],value=2));self.assertTrue(self.has(s,op='assert_zone_multiplicity',player=0,zone='graveyard',index=self.rows['Memnite']['csvid'],value=0))
 def test_wave6_public_mass_expand_rejects_unknown_and_empty_selection(self):
  d=self.declaration();p=self.module.spec(d,self.oracle['Forced March'])
  for v in ([],['effect_0','effect_0'],['no_0']):
   with self.assertRaises(ValueError):self.module.expand('Forced March',4470,d,p,v,self.rows,self.oracle)
 def test_wave6_seeds_life_counts_all_affected_artifacts_including_indestructible(self):
  c=self.cases('Seeds of Innocence')[0];s=c['steps']
  for pl in (0,1):
   amount=sum(int(self.oracle[x['name']]['cmc']) for x in c['setup'] if x['zone']=='battlefield' and x['player']==pl and 'Artifact' in self.oracle[x['name']]['type_line'])
   self.assertTrue(self.has(s,op='assert_life',player=pl,value=20+amount))
 def test_wave6_gust_regenerated_artifact_adds_no_destroyed_life(self):
  c=self.cases('Fracturing Gust',['regeneration_0'])[0];s=c['steps'];destroyed=[x for x in c['setup'] if x['zone']=='battlefield' and x['ref']!='regenerator' and ('Artifact' in self.oracle[x['name']]['type_line'] or 'Enchantment' in self.oracle[x['name']]['type_line']) and 'Indestructible' not in self.oracle[x['name']].get('keywords',[])]
  self.assertTrue(self.has(s,op='assert_life',player=0,value=20+2*len(destroyed)));self.assertTrue(self.has(s,op='assert_card',ref='regenerator',property='in_play',value=1));self.assertTrue(self.has(s,op='assert_mana',player=1,color=0,value=28))
 def test_wave6_targeted_mass_only_replaces_actual_deaths_under_target_controller(self):
  c=self.cases('Mogg Infestation',['effect_0'])[0];s=c['steps'];destroyed=[x for x in c['setup'] if x['zone']=='battlefield' and x['player']==1 and 'Creature' in self.oracle[x['name']]['type_line'] and 'Indestructible' not in self.oracle[x['name']].get('keywords',[])]
  self.assertTrue(self.has(s,op='choose_player',player=0,value=1));self.assertTrue(self.has(s,op='assert_zone_multiplicity',player=1,zone='battlefield',index=self.rows['Goblin']['csvid'],value=2*len(destroyed)));self.assertTrue(self.has(s,op='assert_zone_multiplicity',player=0,zone='battlefield',index=self.rows['Goblin']['csvid'],value=0))
 def test_wave6_target_player_shroud_suppresses_whole_replacement(self):
  s=self.cases('Mogg Infestation',['target_shrouded_1'])[0]['steps'];self.assertTrue(self.has(s,op='cast_response',ref='response',value=1));self.assertTrue(self.has(s,op='assert_zone_multiplicity',player=0,zone='battlefield',index=self.rows['Goblin']['csvid'],value=0))
 def test_wave6_numeric_sweeps_have_exact_upper_and_lower_mana_boundaries(self):
  for n in ('Consume the Meek','Culling Sun'):
   s=self.cases(n)[0]['steps'];self.assertTrue(self.has(s,op='assert_zone_multiplicity',player=1,zone='graveyard',index=self.rows['Wall of Spears']['csvid'],value=1));self.assertTrue(self.has(s,op='assert_zone_multiplicity',player=1,zone='battlefield',index=self.rows['Phyrexian Hulk']['csvid'],value=1))
  s=self.cases('Gaze of Granite',['effect_x0_0'])[0]['steps'];self.assertTrue(self.has(s,op='assert_zone_multiplicity',player=1,zone='graveyard',index=self.rows['Memnite']['csvid'],value=1));self.assertTrue(self.has(s,op='assert_zone_multiplicity',player=1,zone='battlefield',index=self.rows['Forest']['csvid'],value=1))
 def test_wave6_nonland_artifact_filter_has_destructible_artifact_land_control(self):
  c=self.cases('Granulate',['scope_boundary_0'])[0];s=c['steps'];self.assertTrue(any(x['name']=='Great Furnace' for x in c['setup']));self.assertTrue(self.has(s,op='assert_zone_multiplicity',player=1,zone='battlefield',index=self.rows['Great Furnace']['csvid'],value=1))
 def test_wave6_destroyed_creature_tokens_receive_current_controller_and_flying_proofs(self):
  c=self.cases('March of Souls',['stolen_0'])[0];s=c['steps']
  for pl in (0,1):
   expected=sum(x['player']==pl and 'Creature' in self.oracle[x['name']]['type_line'] and 'Indestructible' not in self.oracle[x['name']].get('keywords',[]) for x in c['setup'] if x['zone']=='battlefield')
   self.assertTrue(self.has(s,op='assert_zone_multiplicity',player=pl,zone='battlefield',index=self.rows['Spirit']['csvid'],value=expected))
  for variant,w in [('token_flying_0','flyer'),('token_evasion_1','ground_blocker')]:
   s=self.cases('March of Souls',[variant])[0]['steps'];self.assertTrue(self.has(s,op='cast',ref=w,value=1));self.assertEqual(len([x for x in s if x['op']=='advance_turn']),2)


if __name__=='__main__':unittest.main()
