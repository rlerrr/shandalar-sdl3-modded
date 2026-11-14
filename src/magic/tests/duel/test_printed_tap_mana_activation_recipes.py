"""Independent whole-Oracle costs, immediate output and genuine lifecycle guards."""
import copy,json,unittest
from pathlib import Path
from generate_cases import registry
import printed_tap_mana_activation_recipes as m

class PrintedTapManaActivationRecipesTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards'];cls.rows={r['name']:r for r in registry()}
    def declaration(self,n,v='payment_0',a=None):
        s=m.pins()['sources'][n]
        if a is None:a=int(next(iter(s['abilities'])))
        p=m.parse(self.oracle[n],a)
        return dict(family=m.FAMILY,version=2,oracle_text=self.oracle[n]['oracle_text'],params=dict(ability=a,interface=copy.deepcopy(s['abilities'][str(a)]),other_clauses=p['other_clauses']),variants=[v])
    def case(self,n,v='payment_0',a=None):
        d=self.declaration(n,v,a);p=m.spec(d,self.oracle[n]);return m.expand(n,self.rows[n]['csvid'],d,p,[v],self.rows,self.oracle)[0]
    def last(self,c,op,**kw):return [s for s in c['steps'] if s['op']==op and all(s.get(k)==v for k,v in kw.items())][-1]['value']
    def activation(self,c):return next(i for i,s in enumerate(c['steps']) if s['op']=='activate' and s['ref']=='source')
    def test_seventeen_normal_implemented_sources_twentyone_whole_profiles(self):
        data=m.pins();self.assertEqual(len(data['sources']),17);self.assertEqual(sum(len(s['abilities']) for s in data['sources'].values()),21)
        for n,s in data['sources'].items():
            self.assertEqual(s['oracle'],self.oracle[n]);self.assertEqual(s['handler'],self.rows[n]['handler']);self.assertEqual(self.rows[n]['kind'],'card');self.assertNotIn('dummy',s['handler']);self.assertEqual(s['oracle']['layout'],'normal')
    def test_complete_oracle_rejects_partial_effects_and_unpiloted_native_clauses(self):
        for text in ('{T}: Add {G}. Draw a card.','Flying\n{T}: Add {G}.','{T}, Sacrifice this artifact: Add {G}.','{T}: Add {G}{G}.'):
            r=copy.deepcopy(self.oracle['Fellwar Stone']);r['oracle_text']=text
            with self.assertRaises(ValueError):m.parse(r,0 if not text.startswith('Flying') else 1)
    def test_source_costs_are_typed_printed_costs_not_effect_targets(self):
        self.assertEqual(m.parse(self.oracle['Springleaf Drum'],0)['tap_other'],True)
        self.assertEqual(m.parse(self.oracle['Vesper Ghoul'],0)['pay_life'],1)
        self.assertEqual(m.parse(self.oracle['Painted Bluffs'],1)['generic'],1)
        self.assertEqual(m.parse(self.oracle['Pristine Talisman'],0)['gain_life'],1)
    def test_family_version_variants_and_full_independent_interface_fail_closed(self):
        for key,value in [('family','permanent_value_activation'),('version',True),('variants',[]),('variants',['payment_0','payment_0']),('variants',['unknown_0'])]:
            d=self.declaration('Fellwar Stone');d[key]=value
            with self.assertRaises(ValueError):m.spec(d,self.oracle['Fellwar Stone'])
        d=self.declaration('Fellwar Stone');d['params']['interface']['filter_menu']=True
        with self.assertRaises(ValueError):m.spec(d,self.oracle['Fellwar Stone'])
    def test_casting_cost_is_independent_and_no_generic_x_hybrid_fallback(self):
        self.assertEqual(m.casting_mana(self.oracle['Silhana Starfletcher']),[2,0,0,1,0,0]);self.assertEqual(m.casting_mana(self.oracle['Vesper Ghoul']),[2,1,0,0,0,0])
        with self.assertRaises(ValueError):m.casting_mana({'mana_cost':'{X}{G}'})
    def test_mana_receipts_are_immediate_without_nonmana_mode_intent_or_extra_resolve(self):
        for n in m.pins()['sources']:
            c=self.case(n);i=self.activation(c);self.assertEqual(c['steps'][i+1],dict(op='assert_stack',value=0));self.assertFalse(any(s['op'] in ('choose_activation_mode','require_activation_mode') for s in c['steps']))
    def test_payment_and_existing_pool_value_are_genuine_different_transactions(self):
        c=self.case('Pristine Talisman','value_0');p=self.case('Pristine Talisman','payment_0')
        self.assertEqual(self.last(c,'assert_mana',player=0,color=0),4);self.assertEqual(self.last(p,'assert_mana',player=0,color=0),1)
    def test_fellwar_uses_opponent_land_not_card_color_or_own_land(self):
        c=self.case('Fellwar Stone');self.assertEqual(self.last(c,'assert_mana',player=0,color=2),1);self.assertEqual(self.last(c,'assert_mana',player=0,color=3),0)
        self.assertEqual(next(x for x in c['setup'] if x['ref']=='support_0')['player'],1);self.assertEqual(next(x for x in c['setup'] if x['ref']=='support_1')['player'],0)
    def test_multiple_eligible_colors_queue_the_actual_color_not_a_mana_dialog(self):
        c=self.case('Fellwar Stone','palette_choice_1');self.assertIn(dict(op='choose_color',player=1,value=4),c['steps']);self.assertEqual(self.last(c,'assert_mana',player=1,color=4),1)
        self.assertFalse(any(s['op']=='choose_dialog' for s in c['steps']))
    def test_single_output_does_not_queue_unoffered_color_choice(self):
        c=self.case('Fellwar Stone');self.assertFalse(any(s['op']=='choose_color' for s in c['steps']))
    def test_empty_scope_query_does_not_invent_activation_prohibition_or_color(self):
        for n in ('Fellwar Stone','Star Compass','Naga Vitalist'):
            c=self.case(n,'empty_query_0');self.assertEqual(self.last(c,'require_can_activate',ref='source'),1);self.assertFalse(any(s['op']=='activate' and s['ref']=='source' for s in c['steps']));self.assertFalse(any(s['op']=='choose_color' for s in c['steps']))
        for n in ('Fellwar Stone','Star Compass'):
            c=self.case(n,'empty_query_1');self.assertEqual(self.last(c,'can_activate',ref='source'),0);self.assertIs(c['_recipe']['raw_ai_availability']['oracle_legal_activation'],True)
    def test_known_zero_output_source_pays_tap_produces_no_mana(self):
        c=self.case('Fellwar Stone','zero_output_0');self.assertEqual(self.last(c,'assert_card',ref='source',property='tapped'),1);self.assertEqual(self.last(c,'assert_mana_total',player=0),0);self.assertFalse(any(s['op']=='choose_color' for s in c['steps']))
    def test_any_type_includes_real_colorless_wastes_and_color_excludes_it(self):
        c=self.case('Naga Vitalist');self.assertEqual(next(x for x in c['setup'] if x['ref']=='support_0')['name'],'Wastes');self.assertEqual(self.last(c,'assert_mana',player=0,color=0),1)
        self.assertNotIn('color_c_0',m.variants(m.parse(self.oracle['Fellwar Stone'],0)))
    def test_star_compass_real_cast_enters_tapped_then_real_twiddle_untaps(self):
        c=self.case('Star Compass');ss=c['steps'];cast=next(i for i,s in enumerate(ss) if s['op']=='cast' and s['ref']=='source');untap=next(i for i,s in enumerate(ss) if s['op']=='cast' and s['ref']=='entry_untap')
        self.assertLess(cast,untap);self.assertIn(dict(op='require_card',ref='source',property='tapped',value=1),ss[cast:untap]);self.assertLess(untap,self.activation(c))
        denied=self.case('Star Compass','entry_tapped_0');self.assertEqual(self.last(denied,'require_can_activate',ref='source'),0);self.assertFalse(any(s['op']=='cast' and s['ref']=='entry_untap' for s in denied['steps']))
    def test_star_compass_basic_scope_does_not_flatten_own_nonbasic_into_basic(self):
        c=self.case('Star Compass','nonbasic_query_0');self.assertEqual(next(x for x in c['setup'] if x['ref']=='support_0')['name'],'Mana Confluence');self.assertEqual(self.last(c,'assert_mana_total',player=0),0);self.assertEqual(self.last(c,'require_can_activate',ref='source'),1)
    def test_entry_color_choice_is_real_resolution_input_and_source_is_not_an_artifact(self):
        c=self.case('Silhana Starfletcher','color_b_1');ss=c['steps'];cast=next(i for i,s in enumerate(ss) if s['op']=='cast' and s['ref']=='source');choice=next(i for i,s in enumerate(ss) if s['op']=='choose_color')
        self.assertLess(cast,choice);self.assertEqual(ss[choice],dict(op='choose_color',player=1,value=1));self.assertEqual(ss[choice+1]['op'],'resolve');self.assertEqual(self.last(c,'require_card',ref='source',property='power'),1);self.assertEqual(self.last(c,'require_card',ref='source',property='toughness'),3)
    def test_native_reach_actual_flying_block_survives_and_retains_independent_stats(self):
        c=self.case('Silhana Starfletcher','reach_1');self.assertIn(dict(op='block',ref='source',other='flyer',value=1),c['steps']);self.assertEqual(self.last(c,'assert_card',ref='source',property='damage'),1);self.assertEqual(self.last(c,'assert_zone_multiplicity',player=0,zone='graveyard',index=self.rows["Judge's Familiar"]['csvid']),1)
        self.assertEqual(self.last(c,'assert_zone_multiplicity',player=1,zone='battlefield',index=8128),1);self.assertEqual(self.last(c,'assert_mana_total',player=1),0)
    def test_life_gain_is_coupled_to_mana_immediately_and_repeats_without_stack(self):
        c=self.case('Pristine Talisman','repeat_untap_1');i=self.activation(c);nextcast=next(j for j in range(i+1,len(c['steps'])) if c['steps'][j]['op']=='cast')
        self.assertIn(dict(op='assert_life',player=1,value=21),c['steps'][i:nextcast]);self.assertEqual(self.last(c,'assert_life',player=1),22);self.assertEqual(self.last(c,'assert_mana',player=1,color=0),2)
    def test_creature_life_cost_and_safe_exact_life_query_are_not_ai_once_rules(self):
        c=self.case('Vesper Ghoul','life_two_0');self.assertEqual(self.last(c,'assert_life',player=0),1);self.assertEqual(self.last(c,'assert_mana',player=0,color=3),1)
        c=self.case('Vesper Ghoul','life_one_query_1');self.assertEqual(self.last(c,'require_can_activate',ref='source'),1);self.assertFalse(any(s['op']=='activate' and s['ref']=='source' for s in c['steps']))
    def test_real_fresh_creature_tap_denial_has_no_unoffered_color_plan(self):
        for n in ('Naga Vitalist','Vesper Ghoul','Silhana Starfletcher'):
            c=self.case(n,'fresh_0');self.assertEqual(self.last(c,'require_can_activate',ref='source'),0);self.assertEqual(self.last(c,'assert_card',ref='source',property='sick'),1);self.assertFalse(any(s['op']=='activate' and s['ref']=='source' for s in c['steps']))
    def test_drum_extra_cost_can_tap_real_fresh_creature_despite_sickness(self):
        c=self.case('Springleaf Drum','helper_fresh_1');self.assertIn(dict(op='cast',ref='helper',value=1),c['steps']);self.assertEqual(self.last(c,'assert_card',ref='helper',property='sick'),1);self.assertEqual(self.last(c,'assert_card',ref='helper',property='tapped'),1);self.assertEqual(self.last(c,'assert_card',ref='source',property='tapped'),1);self.assertEqual(self.last(c,'assert_mana',player=1,color=3),1)
    def test_drum_human_and_ai_actual_cost_input_precedes_output_color(self):
        for who in (0,1):
            c=self.case('Springleaf Drum',f'payment_{who}');i=self.activation(c);self.assertEqual(c['steps'][i-2:i],[dict(op='choose_target',player=who,ref='helper'),dict(op='choose_color',player=who,value=3)])
    def test_no_owned_untapped_cost_creature_denies_even_with_enemy_creature(self):
        for v in ('no_helper_0','helper_tapped_1'):
            c=self.case('Springleaf Drum',v);self.assertEqual(self.last(c,'require_can_activate',ref='source'),0);self.assertTrue(any(x['ref']=='enemy' for x in c['setup']));self.assertFalse(any(s['op']=='activate' and s['ref']=='source' for s in c['steps']))
    def test_shrouded_helper_cost_is_real_non_target_cost_not_protection_override(self):
        c=self.case('Springleaf Drum','helper_shroud_0');ss=c['steps'];protector=next(i for i,s in enumerate(ss) if s['op']=='activate' and s['ref']=='protector')
        self.assertLess(protector,self.activation(c));self.assertIn(dict(op='choose_target',player=0,ref='sac_land'),ss[:protector]);self.assertEqual(self.last(c,'assert_card',ref='helper',property='tapped'),1)
        self.assertFalse(any(s['op'] in ('set_shroud','set_keyword','set_sick') for s in ss))
    def test_filter_printed_one_cost_spends_single_pool_then_outputs_real_color(self):
        c=self.case('Painted Bluffs','payment_0',1);self.assertEqual(self.last(c,'assert_mana',player=0,color=0),0);self.assertEqual(self.last(c,'assert_mana',player=0,color=3),1);self.assertEqual(self.last(c,'assert_mana_total',player=0),1)
        c=self.case('Painted Bluffs','repeat_untap_0',1);self.assertEqual(self.last(c,'assert_mana_total',player=0),1)
    def test_free_c_sibling_with_budget_chooses_actual_c_option_without_filter_charge(self):
        c=self.case('Painted Bluffs','value_1',0);self.assertIn(dict(op='choose_color',player=1,value=0),c['steps']);self.assertEqual(self.last(c,'assert_mana',player=1,color=0),4)
        c=self.case('Painted Bluffs','filter_unpayable_query_1',1);self.assertEqual(self.last(c,'require_can_activate',ref='source'),1);self.assertFalse(any(s['op']=='activate' and s['ref']=='source' for s in c['steps']))
    def test_produced_mana_is_spent_by_real_one_generic_icy_activation(self):
        c=self.case('Vesper Ghoul','spend_0');self.assertIn(dict(op='activate',ref='spender',value=1),c['steps']);self.assertEqual(self.last(c,'assert_mana_total',player=0),0);self.assertEqual(self.last(c,'assert_card',ref='enemy',property='tapped'),1)
    def test_guarded_untap_has_real_turn_preparation_empty_pools_and_second_actual_mana(self):
        c=self.case('Pristine Talisman','untap_step_1');ss=c['steps'];i=next(i for i,s in enumerate(ss) if s['op']=='untap_step');self.assertEqual([s['op'] for s in ss[i-2:i]],['advance_turn','advance_turn']);self.assertEqual(ss[i+1],dict(op='upkeep_step',player=1));self.assertEqual(self.last(c,'assert_life',player=1),22)
        self.assertTrue(any(s['op']=='activate' and s['ref']=='spender' for s in ss[:i]));self.assertEqual(self.last(c,'assert_mana_total',player=0),0)
    def test_snapshot_replays_actual_mana_cost_color_and_native_state(self):
        c=self.case('Springleaf Drum','ready_snapshot_1');self.assertEqual(sum(s['op']=='activate' and s['ref']=='source' for s in c['steps']),2);self.assertIn(dict(op='assert_recorded_state'),c['steps'])
        c=self.case('Silhana Starfletcher','resolved_snapshot_0');self.assertIn(dict(op='restore'),c['steps']);self.assertEqual(self.last(c,'assert_mana',player=0,color=4),1)
    def test_all_witnesses_are_full_independent_normal_implemented_records(self):
        m.validate_witnesses(self.oracle,self.rows)
        for n,s in m.pins()['witnesses'].items():self.assertEqual(s['oracle'],self.oracle[n]);self.assertEqual(s['oracle']['layout'],'normal');self.assertNotIn('dummy',s['handler'])
        bad=copy.deepcopy(self.oracle);bad['Wastes']['oracle_text']='{T}: Add {G}.'
        with self.assertRaises(ValueError):m.validate_witnesses(bad,self.rows)
    def test_no_primitive_state_setters_and_all_cost_entities_predeclared(self):
        for n in m.pins()['sources']:
            c=self.case(n);refs={x['ref'] for x in c['setup']};self.assertTrue(all(s.get('ref') in refs for s in c['steps'] if s['op']=='choose_target'));self.assertFalse(any(s['op'] in ('set_power','set_toughness','set_counter','set_sick','set_color','set_info_slot') for s in c['steps']))
    def test_actual_human_generic_pool_click_precedes_icy_effect_target(self):
        for n,color in (('Star Compass',3),('Fellwar Stone',2),('Silhana Starfletcher',4),('Vesper Ghoul',3),('Springleaf Drum',3),('Painted Bluffs',3)):
            c=self.case(n,'spend_0',1 if n=='Painted Bluffs' else None);ss=c['steps'];i=next(i for i,s in enumerate(ss) if s['op']=='activate' and s['ref']=='spender')
            self.assertEqual(ss[i-2:i],[dict(op='choose_mana',player=0,color=color),dict(op='choose_target',player=0,ref='enemy')]);self.assertEqual(self.last(c,'assert_mana_total',player=0),0)
            ai=self.case(n,'spend_1',1 if n=='Painted Bluffs' else None);self.assertFalse(any(s['op']=='choose_mana' for s in ai['steps']))
    def test_second_human_filter_click_follows_output_choice_and_keeps_printed_cost(self):
        c=self.case('Painted Bluffs','repeat_untap_0',1);ss=c['steps'];acts=[i for i,s in enumerate(ss) if s['op']=='activate' and s['ref']=='source']
        self.assertEqual(len(acts),2);self.assertEqual(ss[acts[1]-2:acts[1]],[dict(op='choose_color',player=0,value=3),dict(op='choose_mana',player=0,color=3)])
        self.assertEqual(sum(s['op']=='choose_mana' for s in ss),1);self.assertEqual(self.last(c,'assert_mana',player=0,color=3),1);self.assertEqual(m.parse(self.oracle['Painted Bluffs'],1)['generic'],1)
    def test_thirteen_raw_ai_policy_cells_do_not_force_action_or_change_oracle_legality(self):
        total=0
        for n,s in m.pins()['sources'].items():
            for variant,policy in s.get('raw_ai_query_policy',{}).items():
                c=self.case(n,variant);total+=1;self.assertEqual(self.last(c,'can_activate',ref='source'),0);self.assertIs(policy['oracle_legal_activation'],True)
                self.assertFalse(any(st['op']=='activate' and st['ref']=='source' for st in c['steps']));self.assertEqual(self.last(c,'assert_card',ref='source',property='tapped'),0);self.assertEqual(self.last(c,'assert_mana_total',player=1),0)
            if s.get('raw_ai_query_policy'):
                productive=self.case(n,'payment_1');self.assertEqual(self.last(productive,'require_can_activate',ref='source'),1);self.assertEqual(self.last(productive,'assert_mana_total',player=1),1)
                human=self.case(n,'zero_output_0');self.assertEqual(self.last(human,'assert_card',ref='source',property='tapped'),1);self.assertEqual(self.last(human,'assert_mana_total',player=0),0)
        self.assertEqual(total,13)
    def test_measured_fixture_annotations_reject_rule_normalization_and_unseen_policy(self):
        source=copy.deepcopy(m.pins()['sources']['Fellwar Stone']);p=m.parse(self.oracle['Fellwar Stone'],0)
        for change in ({'oracle_legal_activation':False},{'raw_value':True},{'action':'activate'},{'zero_output':False}):
            bad=copy.deepcopy(source);bad['raw_ai_query_policy']['empty_query_1'].update(change)
            with self.assertRaises(ValueError):m.validate_fixture_inputs(p,bad)
        bad=copy.deepcopy(source);bad['raw_ai_query_policy']['payment_1']=bad['raw_ai_query_policy'].pop('empty_query_1')
        with self.assertRaises(ValueError):m.validate_fixture_inputs(p,bad)
        bad=copy.deepcopy(source);bad['generic_payment_inputs']={'0':{'spend_1':{'spender':2}}}
        with self.assertRaises(ValueError):m.validate_fixture_inputs(p,bad)
    def test_quirion_fixed_green_is_distinct_from_real_other_entry_color(self):
        for who in (0,1):
            c=self.case('Quirion Elves',f'entry_other_color_{who}',1);ss=c['steps'];i=self.activation(c)
            choices=[s for s in ss[:i] if s['op']=='choose_color'];self.assertEqual(choices,[dict(op='choose_color',player=who,value=2),dict(op='choose_color',player=who,value=3)])
            self.assertEqual(self.last(c,'assert_mana',player=who,color=3),1);self.assertEqual(self.last(c,'assert_mana',player=who,color=2),0)
    def test_quirion_chosen_green_coalesces_while_other_chosen_color_has_real_menu(self):
        same=self.case('Quirion Elves','color_g_1',2);different=self.case('Quirion Elves','payment_1',2)
        self.assertEqual([s['value'] for s in same['steps'] if s['op']=='choose_color'],[3]);self.assertEqual([s['value'] for s in different['steps'] if s['op']=='choose_color'],[4,4]);self.assertEqual(self.last(different,'assert_mana',player=1,color=4),1)
    def test_settlement_colored_cost_selects_color_then_fresh_helper_for_both_owners(self):
        for n in ('Holdout Settlement',"Survivors' Encampment"):
            for who in (0,1):
                c=self.case(n,f'helper_fresh_{who}',1);ss=c['steps'];i=self.activation(c)
                self.assertEqual(ss[i-2:i],[dict(op='choose_color',player=who,value=3),dict(op='choose_target',player=who,ref='helper')]);self.assertEqual(self.last(c,'assert_card',ref='helper',property='sick'),1);self.assertEqual(self.last(c,'assert_card',ref='helper',property='tapped'),1)
    def test_settlement_free_c_with_helper_never_pays_other_creature_cost(self):
        for n in ('Holdout Settlement',"Survivors' Encampment"):
            for who in (0,1):
                c=self.case(n,f'free_with_helper_{who}',0);self.assertIn(dict(op='choose_color',player=who,value=0),c['steps']);self.assertFalse(any(s['op']=='choose_target' for s in c['steps']))
                self.assertEqual(self.last(c,'assert_card',ref='helper',property='tapped'),0);self.assertEqual(self.last(c,'assert_card',ref='source',property='tapped'),1);self.assertEqual(self.last(c,'assert_mana',player=who,color=0),1)
    def test_free_c_global_query_never_becomes_selected_colored_cost_legality(self):
        for n in ('Holdout Settlement',"Survivors' Encampment"):
            for v in ('no_helper_0','helper_tapped_1'):
                c=self.case(n,v,1);self.assertEqual(self.last(c,'require_can_activate',ref='source'),1);self.assertFalse(any(s['op']=='activate' and s['ref']=='source' for s in c['steps']));self.assertTrue(any('distinct freeC sibling' in g for g in c['_recipe']['untested_components']))
        self.assertEqual(self.last(self.case('Springleaf Drum','no_helper_0'),'require_can_activate',ref='source'),0)
    def test_new_protected_cost_gap_is_explicit_and_does_not_queue_illegal_target(self):
        for n in ('Holdout Settlement',"Survivors' Encampment"):
            c=self.case(n,'helper_shroud_0',1);meta=c['_recipe']['oracle_cost_eligibility'];self.assertIs(meta['shrouded_own_untapped_creature'],True);self.assertIs(meta['is_target'],False)
            self.assertIn('global freeC',meta['executed_observation']);self.assertFalse(any(s['op']=='activate' and s['ref']=='source' for s in c['steps']));self.assertTrue(any('protected-helper selection/action is unexecuted' in g for g in c['_recipe']['untested_components']))
        self.assertTrue(any(s['op']=='activate' and s['ref']=='source' for s in self.case('Loam Dryad','helper_shroud_0')['steps']))
    def test_survivors_ai_shroud_records_real_source_sacrifice_without_retired_query(self):
        c=self.case("Survivors' Encampment",'helper_shroud_1',1);ss=c['steps']
        self.assertIn(dict(op='activate',ref='protector',value=1),ss);self.assertIn(dict(op='assert_stack',value=1),ss)
        self.assertFalse(any(s['op']=='choose_target' and s['ref']=='sac_land' for s in ss))
        self.assertFalse(any(s['op'] in ('can_activate','require_can_activate','activate') and s['ref']=='source' for s in ss))
        self.assertEqual(self.last(c,'require_card',ref='source',property='in_play'),0);self.assertEqual(self.last(c,'require_card',ref='sac_land',property='in_play'),1)
        self.assertEqual(self.last(c,'assert_zone_multiplicity',player=1,zone='graveyard',index=17234),1);self.assertEqual(self.last(c,'assert_zone_multiplicity',player=1,zone='graveyard',index=188),1)
        self.assertEqual(self.last(c,'assert_zone_multiplicity',player=1,zone='battlefield',index=188),1)
        self.assertTrue(any(s['op']=='assert_choices_consumed' for s in ss));self.assertIs(c['_recipe']['oracle_cost_eligibility']['shrouded_own_untapped_creature'],True)
        self.assertIn('Source-present/freeC query',c['_recipe']['oracle_cost_eligibility']['executed_observation']);self.assertEqual(c['_recipe']['observed_ai_sacrifice']['destination'],'owner_graveyard')
        for n,v in (("Survivors' Encampment",'helper_shroud_0'),('Holdout Settlement','helper_shroud_1')):
            mirror=self.case(n,v,1);self.assertEqual(self.last(mirror,'require_can_activate',ref='source'),1);self.assertEqual(self.last(mirror,'require_card',ref='source',property='in_play'),1);self.assertNotIn('observed_ai_sacrifice',mirror['_recipe'])
    def test_measured_source_sacrifice_annotation_is_exact_and_never_a_rules_gate(self):
        s=copy.deepcopy(m.pins()['sources']["Survivors' Encampment"]);p=m.parse(self.oracle["Survivors' Encampment"],1)
        for value in ({},[],{'0':{'helper_shroud_1':'source'}},{'1':{'helper_shroud_0':'source'}},{'1':{'helper_shroud_1':'sac_land'}}):
            bad=copy.deepcopy(s);bad['observed_ai_sacrifices']=value
            with self.assertRaises(ValueError):m.validate_fixture_inputs(p,bad)
        bad=copy.deepcopy(s);bad['csvid']=15614
        with self.assertRaises(ValueError):m.validate_fixture_inputs(p,bad)
    def test_coldsteel_compounds_real_color_entry_tapped_and_snow_identity(self):
        c=self.case('Coldsteel Heart','color_u_0');self.assertEqual(self.oracle['Coldsteel Heart']['type_line'],'Snow Artifact');self.assertIn(dict(op='require_card',ref='source',property='tapped',value=1),c['steps']);self.assertIn(dict(op='cast',ref='entry_untap',value=1),c['steps']);self.assertEqual(self.last(c,'assert_mana',player=0,color=2),1)
    def test_reflecting_pool_uses_real_land_fixture_and_wastes_type_without_casting_land(self):
        c=self.case('Reflecting Pool');self.assertEqual(next(x for x in c['setup'] if x['ref']=='source')['zone'],'battlefield');self.assertFalse(any(s['op']=='cast' and s['ref']=='source' for s in c['steps']));self.assertEqual(self.last(c,'assert_mana',player=0,color=0),1)

if __name__=='__main__':unittest.main()
