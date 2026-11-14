"""Independent Oracle effects, real cost transactions and combat contrasts."""
import copy,json,unittest
from pathlib import Path
from generate_cases import registry
import activated_temporary_modifier_recipes as m

class ActivatedTemporaryModifierRecipesTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards'];cls.rows={r['name']:r for r in registry()}
    def declaration(self,n,v='effect_0',a=None):
        pin=m.pins()['sources'][n]
        if a is None:a=int(next(iter(pin['abilities'])))
        p=m.parse(self.oracle[n],a)
        return dict(family=m.FAMILY,version=2,oracle_text=self.oracle[n]['oracle_text'],params=dict(ability=a,interface=copy.deepcopy(pin['abilities'][str(a)]),other_clauses=p['other_clauses']),variants=[v])
    def case(self,n,v='effect_0',a=None):
        d=self.declaration(n,v,a);p=m.spec(d,self.oracle[n]);return m.expand(n,self.rows[n]['csvid'],d,p,[v],self.rows,self.oracle)[0]
    def last(self,c,op,**fields):return [s for s in c['steps'] if s['op']==op and all(s.get(k)==v for k,v in fields.items())][-1]['value']
    def pending(self,c):
        ss=c['steps'];a=next(i for i,s in enumerate(ss) if s['op']=='activate' and s['ref']=='source');b=next(i for i in range(a+1,len(ss)) if ss[i]['op']=='resolve');return ss[a:b]

    def test_fortyone_normal_implemented_sources_fortyfive_complete_profiles(self):
        data=m.pins();self.assertEqual(len(data['sources']),41);self.assertEqual(sum(len(s['abilities']) for s in data['sources'].values()),45)
        for n,s in data['sources'].items():
            self.assertEqual(s['oracle'],self.oracle[n]);self.assertEqual(s['csvid'],self.rows[n]['csvid']);self.assertEqual(self.rows[n]['kind'],'card');self.assertNotIn('dummy',self.rows[n]['handler'])
    def test_every_new_witness_is_implemented_normal_and_independently_pinned(self):
        m.validate_witnesses(self.oracle,self.rows)
        for n,pin in m.pins()['witnesses'].items():
            self.assertEqual(pin['oracle'],self.oracle[n]);self.assertEqual(pin['oracle']['layout'],'normal');self.assertNotIn('dummy',self.rows[n]['handler'])
    def test_compound_oracle_preserves_signed_deltas_and_all_three_matrix_keywords(self):
        p=m.parse(self.oracle['Flowstone Channeler'],0);self.assertEqual((p['power'],p['toughness'],p['grants']),(1,-1,['Haste']));self.assertEqual(p['cost']['discard'],1);self.assertTrue(p['cost']['tap'])
        p=m.parse(self.oracle['Power Matrix'],0);self.assertEqual((p['power'],p['toughness']),(1,1));self.assertEqual(p['grants'],['Flying','First strike','Trample'])
    def test_life_cost_is_oracle_printed_and_ai_heuristic_never_becomes_once_expectation(self):
        p=m.parse(self.oracle['Shadowcloak Vampire'],0);self.assertEqual(p['cost']['pay_life'],2);self.assertFalse(p['once'])
        for who in (0,1):
            c=self.case('Shadowcloak Vampire',f'repeat_{who}');self.assertEqual(sum(s['op']=='activate' and s['ref']=='source' for s in c['steps']),2);self.assertEqual(self.last(c,'assert_life',player=who),16)
    def test_life_is_paid_while_effect_is_pending_with_unchanged_source_stats(self):
        c=self.case('Shadowcloak Vampire');p=self.pending(c)
        self.assertIn(dict(op='assert_life',player=0,value=18),p);self.assertIn(dict(op='require_card',ref='source',property='power',value=4),p)
    def test_safe_low_life_and_exact_minimum_query_are_different_real_controls(self):
        self.assertEqual(self.last(self.case('Shadowcloak Vampire','low_life_1'),'assert_life',player=1),1)
        c=self.case('Shadowcloak Vampire','insufficient_life_0');self.assertEqual(self.last(c,'require_can_activate',ref='source'),0);self.assertEqual(self.last(c,'assert_life',player=0),1)
        c=self.case('Shadowcloak Vampire','minimum_life_query_0');self.assertEqual(self.last(c,'require_can_activate',ref='source'),1);self.assertFalse(any(s['op']=='activate' and s['ref']=='source' for s in c['steps']));self.assertEqual(self.last(c,'assert_life',player=0),2)
    def test_guildmage_real_modes_and_printed_costs_are_both_enrolled(self):
        for a,label,cost in ((0,'Give Haste',[1,0,0,0,1,0]),(1,'Give First Strike',[1,0,0,0,0,1])):
            p=m.parse(self.oracle['Boros Guildmage'],a);self.assertEqual(p['activation_mana'],cost)
            c=self.case('Boros Guildmage','payment_1',a);self.assertIn(dict(op='choose_activation_mode',ref='source',player=1,mode_label=label,fallback_color=0,allow_auto_nonmana=False),c['steps'])
            self.assertEqual(self.last(c,'assert_mana_total',player=1),0)
    def test_hybrid_cast_pays_two_legal_red_not_arbitrary_mixed_or_colorless(self):
        c=self.case('Boros Guildmage');ss=c['steps'];cast=next(i for i,s in enumerate(ss) if s['op']=='cast' and s['ref']=='source')
        self.assertEqual([s['value'] for s in ss[:cast] if s['op']=='set_mana'][-6:],[0,0,0,0,2,0])
    def test_discard_input_is_target_then_actual_hand_choice_with_pending_cost_receipt(self):
        c=self.case('Flowstone Channeler','payment_0');ss=c['steps'];a=next(i for i,s in enumerate(ss) if s['op']=='activate' and s['ref']=='source')
        self.assertEqual([s['op'] for s in ss[:a] if s['op'].startswith('choose_')][-2:],['choose_target','choose_zone_card'])
        self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='graveyard',index=self.rows['Plains']['csvid'],value=1),self.pending(c));self.assertIn(dict(op='require_card',ref='ally',property='power',value=2),self.pending(c))
    def test_empty_hand_denial_does_not_script_an_unpayable_discard(self):
        c=self.case('Flowstone Channeler','empty_hand_0');self.assertEqual(self.last(c,'require_can_activate',ref='source'),0);self.assertFalse(any(s['op']=='choose_zone_card' for s in c['steps']))
    def test_haste_receipt_uses_real_fresh_recipient_cast_not_a_sickness_setter(self):
        c=self.case('Boros Guildmage','effect_0',0);ss=c['steps'];ally_cast=next(i for i,s in enumerate(ss) if s['op']=='cast' and s['ref']=='ally');activate=next(i for i,s in enumerate(ss) if s['op']=='activate' and s['ref']=='source')
        self.assertLess(ally_cast,activate);self.assertEqual(next(s for s in c['setup'] if s['ref']=='ally')['zone'],'hand')
        self.assertEqual(self.last(c,'attack',ref='ally'),1);self.assertEqual(self.last(c,'assert_life',player=1),18)
        self.assertEqual(self.last(self.case('Boros Guildmage','inactive_0',0),'attack',ref='ally'),0)
    def test_haste_effective_sickness_observer_does_not_erase_retained_age_or_expiry_denial(self):
        c=self.case('Boros Guildmage','effect_0',0)
        self.assertIn(dict(op='assert_card',ref='ally',property='sick',value=1),self.pending(c))
        self.assertEqual(self.last(c,'assert_card',ref='ally',property='sick'),0)
        c=self.case('Boros Guildmage','cleanup_0',0)
        self.assertEqual(self.last(c,'assert_card',ref='ally',property='sick'),1)
        self.assertEqual(self.last(c,'attack',ref='ally'),0)
        self.assertEqual(self.last(c,'require_card',ref='ally',property='power'),2)
    def test_first_strike_changes_real_damage_before_retaliation(self):
        active=self.case('Patrol Hound');inactive=self.case('Patrol Hound','inactive_0')
        self.assertEqual(self.last(active,'assert_card',ref='source',property='damage'),0);self.assertEqual(self.last(inactive,'assert_card',ref='source',property='damage'),1)
        self.assertEqual(self.last(active,'assert_zone_multiplicity',player=1,zone='graveyard',index=self.rows['Sage of Lat-Nam']['csvid']),1)
    def test_deathtouch_kills_real_five_toughness_witness_and_source_dies_simultaneously(self):
        active=self.case('Death-Hood Cobra','effect_0',1);inactive=self.case('Death-Hood Cobra','inactive_0',1)
        self.assertEqual(self.last(active,'assert_zone_multiplicity',player=1,zone='graveyard',index=14370),1);self.assertEqual(self.last(inactive,'assert_zone_multiplicity',player=1,zone='graveyard',index=14370),0)
        self.assertEqual(self.last(active,'assert_zone_multiplicity',player=0,zone='graveyard',index=11587),1)
    def test_reach_enables_actual_flying_block_and_expiry_rejects_it(self):
        self.assertEqual(self.last(self.case('Death-Hood Cobra','effect_1',0),'block',ref='source'),1)
        self.assertEqual(self.last(self.case('Death-Hood Cobra','cleanup_1',0),'block',ref='source'),0)
    def test_reach_remains_ground_blockable_while_flying_evades_real_ground_blocker(self):
        self.assertEqual(self.last(self.case('Death-Hood Cobra','ground_block_0',0),'block',ref='combat_witness'),1)
        self.assertEqual(self.last(self.case('Beetleform Mage','ground_block_1'),'block',ref='combat_witness'),0)
        c=self.case('Power Matrix','ground_block_0');self.assertEqual(self.last(c,'block',ref='combat_witness'),0);self.assertEqual(self.last(c,'assert_life',player=1),17)
    def test_matrix_triple_grant_is_coupled_to_stats_first_strike_and_trample_damage(self):
        c=self.case('Power Matrix','keyword_trample_0');self.assertEqual(self.last(c,'assert_life',player=1),18);self.assertEqual(self.last(c,'assert_card',ref='ally',property='damage'),0)
        self.assertEqual(self.last(c,'require_card',ref='ally',property='power'),3);self.assertEqual(self.last(c,'require_card',ref='ally',property='toughness'),6)
    def test_skarrg_real_mana_sibling_is_executed_without_nonmana_intent_or_fake_cast(self):
        c=self.case('Skarrg, the Rage Pits','mana_sibling_0');self.assertFalse(any(s['op']=='cast' and s['ref']=='source' for s in c['steps']));self.assertFalse(any(s['op']=='choose_activation_mode' for s in c['steps']))
        self.assertEqual(self.last(c,'assert_mana_total',player=0),1);self.assertEqual(self.last(c,'assert_card',ref='source',property='tapped'),1);self.assertNotIn('{T}: Add {C}.',c['_recipe']['untested_components'])
        self.assertEqual([self.last(c,'assert_mana',player=0,color=i) for i in range(6)],[1,0,0,0,0,0])
    def test_skarrg_selected_cost_is_true_rg_and_mode_receipt_scoped_unique(self):
        c=self.case('Skarrg, the Rage Pits','payment_1');self.assertIn(dict(op='choose_activation_mode',ref='source',player=1,mode_label='Pump a creature',fallback_color=0,allow_auto_nonmana=True),c['steps'])
        self.assertEqual(m.parse(self.oracle['Skarrg, the Rage Pits'],1)['activation_mana'],[0,0,0,1,1,0]);self.assertEqual(self.last(c,'assert_mana_total',player=1),0)
    def test_selected_unpayable_skarrg_keeps_real_global_mana_permission(self):
        for v in ('insufficient_mana_0','wrong_color_1'):
            c=self.case('Skarrg, the Rage Pits',v);self.assertEqual(self.last(c,'require_can_activate',ref='source'),1);self.assertFalse(any(s['op']=='activate' and s['ref']=='source' for s in c['steps']))
    def test_skarrg_noncreature_target_probe_uses_real_pump_dialog_instead_of_implicit_mana(self):
        c=self.case('Skarrg, the Rage Pits','probe_noncreature_0')
        self.assertIn(dict(op='probe_activation_target',ref='source',other='invalid',index=1,value=0),c['steps'])
    def test_mode_receipt_immediately_follows_actual_activation_without_cost_or_oracle_override(self):
        c=self.case('Death-Hood Cobra','effect_1',1);ss=c['steps'];a=next(i for i,s in enumerate(ss) if s['op']=='activate' and s['ref']=='source')
        self.assertEqual(ss[a+1],dict(op='require_activation_mode',value=1));self.assertFalse(next(s for s in ss if s['op']=='choose_activation_mode')['allow_auto_nonmana'])
    def test_guildmage_other_mode_budget_does_not_assert_selected_mode_globally_illegal(self):
        for a in (0,1):
            c=self.case('Boros Guildmage','other_mode_only_0',a);self.assertEqual(self.last(c,'require_can_activate',ref='source'),1);self.assertFalse(any(s['op']=='activate' and s['ref']=='source' for s in c['steps']))
    def test_printed_once_limit_denies_repeat_but_real_turn_reset_allows_new_effect(self):
        p=m.parse(self.oracle['Beetleform Mage'],0);self.assertTrue(p['once'])
        c=self.case('Beetleform Mage','repeat_0');self.assertEqual(self.last(c,'require_can_activate',ref='source'),0)
        c=self.case('Beetleform Mage','once_reset_1');self.assertEqual(sum(s['op']=='activate' and s['ref']=='source' and s['value']==1 for s in c['steps']),2);self.assertEqual(self.last(c,'require_card',ref='source',property='power'),4)
    def test_persistent_counter_survives_cleanup_while_both_temporary_stats_and_flying_expire(self):
        c=self.case('Beetleform Mage','counter_cleanup_0');self.assertEqual(self.last(c,'assert_counter',ref='source',counter_kind='plus1'),1);self.assertEqual(self.last(c,'require_card',ref='source',property='power'),3);self.assertEqual(self.last(c,'block',ref='source'),0)
    def test_real_untap_two_paid_negative_modifiers_cause_zero_toughness_sba(self):
        c=self.case('Flowstone Channeler','zero_toughness_1');self.assertIn(dict(op='cast',ref='untap',value=1),c['steps'])
        self.assertEqual(self.last(c,'assert_zone_multiplicity',player=1,zone='graveyard',index=self.rows['Sage of Lat-Nam']['csvid']),1);self.assertEqual(self.last(c,'assert_zone_multiplicity',player=1,zone='graveyard',index=self.rows['Plains']['csvid']),2)
    def test_external_target_keeps_grant_when_source_removed_pending(self):
        c=self.case('Boros Guildmage','source_removed_pending_1',0);self.assertEqual(self.last(c,'attack',ref='ally'),1);self.assertEqual(self.last(c,'assert_zone_multiplicity',player=1,zone='graveyard',index=7779),1)
    def test_removed_recipient_gets_no_future_combat_or_modifier(self):
        c=self.case('Power Matrix','target_removed_pending_0');self.assertFalse(any(s['op']=='begin_attackers' for s in c['steps']));self.assertEqual(self.last(c,'assert_zone_multiplicity',player=0,zone='graveyard',index=14370),1)
    def test_pending_snapshot_replays_real_costed_keyword_combat_not_a_static_checksum_only(self):
        c=self.case('Death-Hood Cobra','pending_snapshot_0',1);ss=c['steps'];capture=next(i for i,s in enumerate(ss) if s['op']=='capture')
        self.assertTrue(any(s['op']=='activate' and s['ref']=='source' for s in ss[:capture]));self.assertEqual(sum(s['op']=='combat_damage' for s in ss[capture:]),2);self.assertIn(dict(op='assert_recorded_state'),ss)
    def test_schemas_and_new_semantic_branches_fail_closed(self):
        for k,v in [('family','activated_temporary_pt'),('version',True),('variants',[]),('variants',['effect_0','effect_0']),('variants',['unknown_0']),('variants',[1])]:
            d=self.declaration('Patrol Hound');d[k]=v
            with self.assertRaises(ValueError):m.spec(d,self.oracle['Patrol Hound'])
        r=copy.deepcopy(self.oracle['Patrol Hound']);r['oracle_text']='Discard a card: This creature gains menace until end of turn.'
        with self.assertRaises(ValueError):m.parse(r,0)
        d=self.declaration('Boros Guildmage');d['params']['interface']['allow_auto_nonmana']=True
        with self.assertRaises(ValueError):m.spec(d,self.oracle['Boros Guildmage'])
    def test_no_stat_counter_or_sickness_setters_and_all_future_entities_predeclared(self):
        for n in m.pins()['sources']:
            c=self.case(n);self.assertFalse(any(s['op'] in ('set_power','set_toughness','set_counter','add_counter','set_sick') for s in c['steps']));self.assertTrue(all('state' in x for x in c['setup'] if x['zone']=='battlefield'))
    def test_independent_witness_and_handler_drift_fail_before_case_expansion(self):
        changed=copy.deepcopy(self.oracle);changed['Scion of Glaciers']['power']='9'
        with self.assertRaises(ValueError):m.validate_witnesses(changed,self.rows)
        changed=copy.deepcopy(self.rows);changed['Scion of Glaciers']['handler']='card_other'
        with self.assertRaises(ValueError):m.validate_witnesses(self.oracle,changed)
    def test_icatian_one_power_first_strike_has_real_one_toughness_contrary_combat(self):
        for who in (0,1):
            active=self.case('Icatian Infantry',f'effect_{who}');inactive=self.case('Icatian Infantry',f'inactive_{who}')
            self.assertEqual(next(x for x in active['setup'] if x['ref']=='combat_witness')['name'],'Elvish Mystic')
            self.assertEqual(self.last(active,'assert_zone_multiplicity',player=who,zone='battlefield',index=1796),1)
            self.assertEqual(self.last(active,'assert_card',ref='source',property='damage'),0)
            self.assertEqual(self.last(inactive,'assert_zone_multiplicity',player=who,zone='graveyard',index=1796),1)
            self.assertEqual(self.last(active,'assert_zone_multiplicity',player=1-who,zone='graveyard',index=13284),1)
    def test_bant_flying_retains_true_oracle_u_even_if_source_offers_wrong_color_gate(self):
        p=m.parse(self.oracle['Bant Battlemage'],1);self.assertEqual(p['activation_mana'],[0,0,1,0,0,0])
        c=self.case('Bant Battlemage','payment_1',1)
        self.assertIn(dict(op='choose_activation_mode',ref='source',player=1,mode_label='Give Flying',fallback_color=0,allow_auto_nonmana=False),c['steps'])
        self.assertEqual(self.last(c,'assert_mana_total',player=1),0)
    def test_new_free_negative_power_and_negative_zero_keep_full_signed_oracle_clause(self):
        p=m.parse(self.oracle['Hyalopterous Lemure'],0);self.assertEqual((p['power'],p['toughness'],p['activation_mana']),(-1,0,[0]*6))
        p=m.parse(self.oracle['Leaping Lizard'],0);self.assertEqual((p['power'],p['toughness']),(0,-1))
    def test_exact_three_released_followers_have_independent_single_profile_pins(self):
        for nm,cid in (('Need for Speed',5634),('Oxidda Daredevil',11327),('Gnathosaur',11450)):
            s=m.pins()['sources'][nm];self.assertEqual(s['csvid'],cid);self.assertEqual(set(s['abilities']),{'0'});self.assertNotIn('measured_cost_victims',s)

    def test_delta_typed_costs_come_from_complete_independent_oracle(self):
        for nm,sac,vector in [('Sylvan Safekeeper','land',[0]*6),('Megatog','artifact',[0]*6),('Dross Hopper','creature',[0]*6),('Vial of Poison','self',[1,0,0,0,0,0])]:
            p=m.parse(self.oracle[nm],0);self.assertEqual(p['cost']['sacrifice'],sac);self.assertEqual(p['activation_mana'],vector)
        for text in ('Sacrifice two creatures: This creature gains flying until end of turn.','Remove a counter: This creature gains flying until end of turn.'):
            r=copy.deepcopy(self.oracle['Dross Hopper']);r['oracle_text']=text
            with self.assertRaises(ValueError):m.parse(r,0)
    def test_safekeeper_cost_land_is_separate_and_after_actual_own_effect_target(self):
        c=self.case('Sylvan Safekeeper');ss=c['steps'];a=next(i for i,s in enumerate(ss) if s['op']=='activate' and s['ref']=='source')
        self.assertEqual([s['ref'] for s in ss[:a] if s['op']=='choose_target'][-2:],['ally','cost_0'])
        self.assertEqual(m.parse(self.oracle['Sylvan Safekeeper'],0)['scope'],'own')
        self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='graveyard',index=self.rows['Plains']['csvid'],value=1),self.pending(c))
        self.assertIn(dict(op='probe_activation_target',ref='source',other='enemy',index=-1,value=0),ss)
    def test_shroud_rejects_friendly_and_enemy_real_spell_targets_and_expires(self):
        active=self.case('Sylvan Safekeeper');expired=self.case('Sylvan Safekeeper','cleanup_1');inactive=self.case('Sylvan Safekeeper','inactive_0')
        for who in (0,1):
            self.assertIn(dict(op='can_target_creature',ref=f'probe_{who}',other='ally',value=0),active['steps'])
            self.assertIn(dict(op='can_target_creature',ref=f'probe_{who}',other='ally',value=1),expired['steps'])
            self.assertIn(dict(op='can_target_creature',ref=f'probe_{who}',other='ally',value=1),inactive['steps'])
        self.assertTrue(all(next(x for x in active['setup'] if x['ref']==f'probe_{who}')['zone']=='hand' for who in (0,1)))
    def test_shroud_repeat_uses_distinct_legal_own_recipient_not_protected_target(self):
        c=self.case('Sylvan Safekeeper','repeat_0');ss=c['steps'];activations=[i for i,s in enumerate(ss) if s['op']=='activate' and s['ref']=='source']
        self.assertEqual(len(activations),2);self.assertEqual([s['ref'] for s in ss[:activations[1]] if s['op']=='choose_target'][-2:],['source','cost_1'])
        self.assertEqual(self.last(c,'assert_zone_multiplicity',player=0,zone='battlefield',index=self.rows['Plains']['csvid']),1)
    def test_missing_land_and_artifact_deny_cost_without_fake_fallback_action(self):
        for n in ('Sylvan Safekeeper','Megatog'):
            c=self.case(n,'no_resource_0');self.assertIn(dict(op='require_can_activate',ref='source',value=0),c['steps']);self.assertIn(dict(op='activate',ref='source',value=0),c['steps'])
            self.assertFalse(any(s['op']=='choose_target' for s in c['steps']))
    def test_megatog_actual_artifact_cost_precedes_coupled_stats_and_trample(self):
        c=self.case('Megatog');p=self.pending(c)
        self.assertIn(dict(op='require_card',ref='source',property='power',value=3),p);self.assertIn(dict(op='require_card',ref='source',property='toughness',value=4),p)
        self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='graveyard',index=2346,value=1),p)
        self.assertEqual(self.last(c,'require_card',ref='source',property='power'),6);self.assertEqual(self.last(c,'require_card',ref='source',property='toughness'),7);self.assertEqual(self.last(c,'assert_life',player=1),15)
        self.assertFalse(any(x['name']=='Icy Manipulator' and x['player']==0 and x['zone']=='battlefield' for x in c['setup']))
    def test_dross_source_itself_is_legal_cost_and_implicit_self_effect_fizzles(self):
        for who in (0,1):
            c=self.case('Dross Hopper',f'source_sacrifice_{who}');p=self.pending(c)
            self.assertIn(dict(op='require_can_activate',ref='source',value=1),c['steps']);self.assertIn(dict(op='assert_zone_multiplicity',player=who,zone='graveyard',index=11242,value=1),p)
            self.assertFalse(any(s['op']=='require_card' and s['ref']=='source' for s in p));self.assertFalse(any(s['op']=='begin_attackers' for s in c['steps']))
            self.assertFalse(any(x['name']=='Shield Sphere' for x in c['setup']))
    def test_dross_other_sacrifice_flying_combat_then_tapped_self_sacrifice(self):
        c=self.case('Dross Hopper','repeat_source_0');ss=c['steps'];acts=[i for i,s in enumerate(ss) if s['op']=='activate' and s['ref']=='source']
        end=next(i for i,s in enumerate(ss) if s['op']=='end_combat');self.assertLess(acts[0],end);self.assertLess(end,acts[1])
        self.assertIn(dict(op='block',ref='combat_witness',other='source',value=0),ss);self.assertEqual(self.last(c,'assert_life',player=1),18)
        self.assertEqual([s['ref'] for s in ss[:acts[1]] if s['op']=='choose_target'][-1],'source');self.assertEqual(self.last(c,'assert_zone_multiplicity',player=0,zone='graveyard',index=11242),1)
    def test_vial_auto_self_sacrifice_is_not_an_extra_target_choice(self):
        c=self.case('Vial of Poison','payment_0');p=self.pending(c)
        self.assertEqual([s['ref'] for s in c['steps'] if s['op']=='choose_target'],['ally']);self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='graveyard',index=13357,value=1),p)
        self.assertIn(dict(op='assert_mana_total',player=0,value=0),p);self.assertEqual(self.last(c,'assert_zone_multiplicity',player=1,zone='graveyard',index=14370),1)
        removed=self.case('Vial of Poison','source_removed_pending_1');self.assertFalse(any(s['op']=='destroy' and s['ref']=='source' for s in removed['steps']))
    def test_double_strike_unblocked_two_packets_and_cleanup_single_packet(self):
        c=self.case('Sunhome, Fortress of the Legion');self.assertEqual(self.last(c,'assert_life',player=1),16)
        self.assertEqual(self.last(self.case('Sunhome, Fortress of the Legion','inactive_0'),'assert_life',player=1),18)
        self.assertEqual(self.last(self.case('Sunhome, Fortress of the Legion','cleanup_0'),'assert_life',player=1),18)
        self.assertEqual(self.last(self.case('Sunhome, Fortress of the Legion','blocked_0'),'assert_card',ref='combat_witness',property='damage'),4)
    def test_sunhome_cost_is_printed_two_rw_not_source_three_rw_availability(self):
        p=m.parse(self.oracle['Sunhome, Fortress of the Legion'],1);self.assertEqual(p['activation_mana'],[2,0,0,0,1,1])
        c=self.case('Sunhome, Fortress of the Legion','payment_1');self.assertIn(dict(op='choose_activation_mode',ref='source',player=1,mode_label='Give double strike',fallback_color=0,allow_auto_nonmana=True),c['steps']);self.assertEqual(self.last(c,'assert_mana_total',player=1),0)
    def test_slayers_whole_fresh_recipient_clause_includes_power_haste_and_vigilance(self):
        c=self.case("Slayers' Stronghold",'payment_1');ss=c['steps'];self.assertIn(dict(op='cast',ref='ally',value=1),ss)
        self.assertEqual(self.last(c,'require_card',ref='ally',property='power'),4);self.assertEqual(self.last(c,'attack',ref='ally'),1);self.assertEqual(self.last(c,'assert_card',ref='ally',property='tapped'),0);self.assertEqual(self.last(c,'assert_card',ref='ally',property='sick'),0);self.assertEqual(self.last(c,'assert_life',player=0),16)
        expired=self.case("Slayers' Stronghold",'counter_cleanup_0');self.assertEqual(self.last(expired,'assert_counter',ref='ally',counter_kind='plus1'),1);self.assertEqual(self.last(expired,'require_card',ref='ally',property='power'),3);self.assertEqual(self.last(expired,'attack',ref='ally'),0);self.assertEqual(self.last(expired,'assert_card',ref='ally',property='sick'),1)
    def test_lifelink_life_belongs_to_recipient_controller_including_enemy(self):
        own=self.case('Vizkopa Guildmage');enemy=self.case('Vizkopa Guildmage','target_enemy_0');blocked=self.case('Vizkopa Guildmage','blocked_0')
        self.assertEqual((self.last(own,'assert_life',player=0),self.last(own,'assert_life',player=1)),(22,18))
        self.assertEqual((self.last(enemy,'assert_life',player=0),self.last(enemy,'assert_life',player=1)),(18,22))
        self.assertEqual((self.last(blocked,'assert_life',player=0),self.last(blocked,'assert_life',player=1)),(22,20))
        self.assertIn(self.oracle['Vizkopa Guildmage']['oracle_text'].splitlines()[1],own['_recipe']['untested_components'])
    def test_brightling_both_real_modes_and_unselected_pt_global_permission(self):
        for a,label in ((0,'Vigilance'),(1,'Lifelink')):
            c=self.case('Brightling','payment_1',a);self.assertIn(dict(op='choose_activation_mode',ref='source',player=1,mode_label=label,fallback_color=0,allow_auto_nonmana=False),c['steps'])
            denied=self.case('Brightling','wrong_color_0',a);self.assertEqual(self.last(denied,'require_can_activate',ref='source'),1);self.assertFalse(any(s['op']=='activate' and s['ref']=='source' for s in denied['steps']))
        self.assertEqual(self.last(self.case('Brightling','effect_1',0),'assert_card',ref='source',property='tapped'),0)
        self.assertEqual(self.last(self.case('Brightling','effect_0',1),'assert_life',player=0),23)
    def test_delta_cost_interface_is_independently_pinned_and_typed(self):
        for field,value in (('sacrifice_before_target',1),('sacrifice_choices',{'0':True,'1':1}),('allow_auto_nonmana',True)):
            d=self.declaration('Sylvan Safekeeper');d['params']['interface'][field]=value
            with self.assertRaises(ValueError):m.spec(d,self.oracle['Sylvan Safekeeper'])
    def test_cost_resource_own_lifecycle_never_invokes_its_dormant_block_trigger(self):
        c=self.case('Megatog','pending_snapshot_1');self.assertEqual(self.oracle['Shield Sphere']['power'],'0');self.assertEqual(self.oracle['Shield Sphere']['toughness'],'6')
        self.assertFalse(any(s['op'] in ('attack','block') and s['ref'].startswith('cost_') for s in c['steps']))
        self.assertEqual(sum(s['op']=='combat_damage' for s in c['steps']),2);self.assertIn(dict(op='assert_recorded_state'),c['steps'])
    def test_measured_dross_ai_cost_does_not_force_unoffered_input_or_preserve_source(self):
        c=self.case('Dross Hopper','effect_1');p=self.pending(c)
        self.assertFalse(any(s['op']=='choose_target' for s in c['steps']))
        self.assertIn(dict(op='assert_zone_multiplicity',player=1,zone='graveyard',index=11242,value=1),p)
        self.assertIn(dict(op='assert_zone_multiplicity',player=1,zone='battlefield',index=2346,value=1),p)
        self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='graveyard',index=11242,value=0),p)
        self.assertEqual(self.last(c,'assert_zone_multiplicity',player=1,zone='graveyard',index=2346),0)
        self.assertFalse(any(s['op']=='begin_attackers' for s in c['steps']));self.assertTrue(any('AI surviving-source Flying' in gap for gap in c['_recipe']['untested_components']))
    def test_measured_dross_stops_only_actions_on_source_actually_paid_as_cost(self):
        for v in ('repeat_1','repeat_source_1','source_removed_pending_1','tapped_1'):
            c=self.case('Dross Hopper',v);ss=c['steps'];a=next(i for i,s in enumerate(ss) if s['op']=='activate' and s['ref']=='source')
            self.assertFalse(any(s.get('ref')=='source' and s['op'] in ('activate','choose_target','destroy','attack','block','require_card','assert_card') for s in ss[a+1:]))
            self.assertEqual(self.last(c,'assert_zone_multiplicity',player=1,zone='graveyard',index=11242),1)
            if v=='tapped_1':self.assertFalse(any(s['op']=='cast' and s['ref']=='untap' for s in ss))
        human=self.case('Dross Hopper','repeat_source_0');self.assertEqual(sum(s['op']=='activate' and s['ref']=='source' for s in human['steps']),2);self.assertEqual(self.last(human,'assert_life',player=1),18)
    def test_measured_dross_pending_snapshot_replays_external_resource_and_owner_zones(self):
        c=self.case('Dross Hopper','pending_snapshot_1');self.assertEqual(sum(s['op']=='activate' and s['ref']=='source' for s in c['steps']),1)
        self.assertEqual(sum(s['op']=='resolve' for s in c['steps']),3);self.assertIn(dict(op='assert_recorded_state'),c['steps'])
        self.assertEqual(self.last(c,'assert_zone_multiplicity',player=1,zone='battlefield',index=2346),1)
        self.assertEqual(self.last(c,'assert_zone_multiplicity',player=1,zone='graveyard',index=11242),1)
    def test_measured_victim_annotation_is_scoped_typed_positive_and_never_oracle_rule(self):
        s=copy.deepcopy(m.pins()['sources']['Dross Hopper']);p=m.parse(self.oracle['Dross Hopper'],0)
        for mapping in ({'effect_0':'source'},{'inactive_1':'source'},{1:'source'},{'effect_1':'cost_0'}):
            bad=copy.deepcopy(s);bad['measured_cost_victims']=mapping
            with self.assertRaises(ValueError):m._validate_measured_cost_victims(p,bad)
        bad=copy.deepcopy(s);bad['abilities']['0']['sacrifice_choices']['1']=True
        with self.assertRaises(ValueError):m._validate_measured_cost_victims(p,bad)
        self.assertEqual(p['cost']['sacrifice'],'creature');self.assertEqual(p['grants'],['Flying']);self.assertEqual(p['activation_mana'],[0]*6)
    def test_need_for_speed_cost_is_free_land_sacrifice_before_independent_target(self):
        p=m.parse(self.oracle['Need for Speed'],0);self.assertEqual(p['activation_mana'],[0]*6);self.assertEqual(p['scope'],'target');self.assertEqual(p['cost']['sacrifice'],'land')
        c=self.case('Need for Speed');ss=c['steps'];a=next(i for i,s in enumerate(ss) if s['op']=='activate' and s['ref']=='source')
        self.assertEqual([s['ref'] for s in ss[:a] if s['op']=='choose_target'][-2:],['cost_0','ally'])
        self.assertIn(dict(op='assert_mana_total',player=0,value=80),self.pending(c));self.assertIn(dict(op='assert_mana',player=0,color=4,value=10),self.pending(c))
        self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='graveyard',index=self.rows['Plains']['csvid'],value=1),self.pending(c))
    def test_need_for_speed_zero_budget_query_does_not_pad_unprinted_red_cost(self):
        for who in (0,1):
            c=self.case('Need for Speed',f'payment_{who}');ss=c['steps'];a=next(i for i,s in enumerate(ss) if s['op']=='require_can_activate' and s['ref']=='source')
            self.assertEqual(ss[a]['value'],1);self.assertEqual([s['value'] for s in ss[:a] if s['op']=='set_mana' and s['player']==who][-6:],[0]*6)
            self.assertEqual(self.last(c,'assert_mana_total',player=who),0);self.assertIn(dict(op='cast',ref='ally',value=1),ss);self.assertEqual(self.last(c,'attack',ref='ally'),1)
    def test_follow_artifact_costs_have_only_real_other_resource_and_no_forced_ai_victim(self):
        for n in ('Oxidda Daredevil','Gnathosaur'):
            c=self.case(n,'payment_1');self.assertNotIn('Artifact',self.oracle[n]['type_line'])
            eligible=[x['name'] for x in c['setup'] if x['zone']=='battlefield' and x['player']==1 and 'Artifact' in self.oracle[x['name']]['type_line']]
            self.assertEqual(eligible,['Shield Sphere']);self.assertFalse(any(s['op']=='choose_target' for s in c['steps']))
            self.assertIn(dict(op='assert_zone_multiplicity',player=1,zone='graveyard',index=2346,value=1),self.pending(c))
    def test_oxidda_real_fresh_source_cast_haste_allows_attack_without_sickness_setter(self):
        for who in (0,1):
            c=self.case('Oxidda Daredevil',f'fresh_{who}');self.assertIn(dict(op='assert_card',ref='source',property='sick',value=1),self.pending(c))
            self.assertEqual(self.last(c,'attack',ref='source'),1);self.assertEqual(self.last(c,'assert_card',ref='source',property='sick'),0);self.assertEqual(self.last(c,'assert_life',player=1-who),18)
            self.assertFalse(any(s['op']=='advance_turn' for s in c['steps']))
    def test_gnathosaur_preserves_independent_five_four_without_megatog_pump(self):
        p=m.parse(self.oracle['Gnathosaur'],0);self.assertEqual((p['power'],p['toughness'],p['grants']),(0,0,['Trample']))
        c=self.case('Gnathosaur');self.assertIn(dict(op='require_card',ref='source',property='power',value=5),self.pending(c));self.assertEqual(self.last(c,'require_card',ref='source',property='power'),5);self.assertEqual(self.last(c,'require_card',ref='source',property='toughness'),4)
        self.assertEqual(self.last(c,'assert_life',player=1),16);self.assertEqual(self.last(self.case('Gnathosaur','inactive_0'),'assert_life',player=1),20)

if __name__=='__main__':unittest.main()
