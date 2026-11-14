"""Independent charge amount, cost, target, lifecycle and replay guards."""
import copy
import json
from pathlib import Path
import unittest
from unittest.mock import patch

from generate_cases import registry
import charge_counter_activation_recipes as model


class ChargeCounterActivationRecipesTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows={r['name']:r for r in registry()}

    def declaration(self,name,variant='charge_0',ability=None):
        source=model.pins()['sources'][name]
        if ability is None: ability=int(next(iter(source['abilities'])))
        return dict(family=model.FAMILY,version=2,oracle_text=self.oracle[name]['oracle_text'],
                    params=dict(ability=ability,interface=copy.deepcopy(source['abilities'][str(ability)]),
                    inactive_clauses=[s for i,s in enumerate(self.oracle[name]['oracle_text'].splitlines()) if i!=ability]),variants=[variant])

    def case(self,name,variant='charge_0',ability=None):
        d=self.declaration(name,variant,ability);p=model.spec(d,self.oracle[name])
        return model.expand(name,self.rows[name]['csvid'],d,p,[variant],self.rows,self.oracle)[0]

    def last(self,c,op,**fields):
        return [s for s in c['steps'] if s['op']==op and all(s.get(k)==v for k,v in fields.items())][-1]['value']

    def pending(self,c):
        s=c['steps'];a=next(i for i,x in enumerate(s) if x['op']=='activate' and x['ref']=='source')
        b=next(i for i in range(a+1,len(s)) if s[i]['op']=='resolve')
        return s[a:b]

    def test_eight_implemented_normal_sources_and_nine_profiles(self):
        sources=model.pins()['sources'];pilot={'Black Mana Battery','Gemstone Array','Coretapper','Culling Dais','Surge Node',
                                              'Trigon of Corruption',"Mirrodin's Core",'Ventifact Bottle'}
        self.assertTrue(pilot<=set(sources));self.assertEqual(sum(len(sources[n]['abilities']) for n in pilot),9)
        for nm,pin in sources.items():
            row=self.rows[nm];self.assertEqual(row['csvid'],pin['csvid']);self.assertEqual(row['kind'],'card')
            self.assertTrue(row['handler']);self.assertNotIn('dummy',row['handler']);self.assertEqual(pin['oracle']['layout'],'normal')

    def test_charge_entry_is_acquired_by_real_cast_not_setters(self):
        for nm,initial in [('Surge Node',6),('Trigon of Corruption',3),('Black Mana Battery',0),('Coretapper',0)]:
            c=self.case(nm);s=c['steps'];cast=next(i for i,x in enumerate(s) if x['op']=='cast')
            activate=next(i for i,x in enumerate(s) if x['op']=='activate' and x['ref']=='source')
            self.assertIn(dict(op='assert_counter',ref='source',counter_kind='charge',value=initial),s[cast:activate])
            self.assertFalse(any(x['op'] in ('set_counter','add_counter','set_power','set_toughness') for x in s))

    def test_fixed_self_counters_increase_only_on_resolution(self):
        for nm,before,after in [('Black Mana Battery',0,1),('Gemstone Array',0,1),('Culling Dais',0,1),
                              ('Trigon of Corruption',3,4),("Mirrodin's Core",0,1)]:
            for owner in (0,1):
                c=self.case(nm,f'charge_{owner}')
                self.assertIn(dict(op='assert_counter',ref='source',counter_kind='charge',value=before),self.pending(c))
                self.assertEqual(self.last(c,'assert_counter',ref='source',counter_kind='charge'),after)

    def test_tap_and_two_charge_self_sacrifice_are_separate_printed_costs(self):
        tapped=self.case('Coretapper','charge_0',0);sacrificed=self.case('Coretapper','charge_0',1)
        self.assertIn(dict(op='require_card',ref='source',property='tapped',value=1),self.pending(tapped))
        self.assertEqual(self.last(tapped,'assert_counter',ref='native',counter_kind='charge'),1)
        self.assertEqual(self.last(sacrificed,'assert_counter',ref='native',counter_kind='charge'),2)
        self.assertEqual(self.last(sacrificed,'assert_zone_multiplicity',player=0,zone='battlefield',index=self.rows['Coretapper']['csvid']),0)
        self.assertEqual(self.last(sacrificed,'assert_zone_multiplicity',player=0,zone='graveyard',index=self.rows['Coretapper']['csvid']),1)

    def test_charge_removal_cost_precedes_target_increment(self):
        for owner in (0,1):
            c=self.case('Surge Node',f'charge_{owner}')
            self.assertIn(dict(op='assert_counter',ref='source',counter_kind='charge',value=5),self.pending(c))
            self.assertIn(dict(op='assert_counter',ref='native',counter_kind='charge',value=0),self.pending(c))
            self.assertEqual(self.last(c,'assert_counter',ref='source',counter_kind='charge'),5)
            self.assertEqual(self.last(c,'assert_counter',ref='native',counter_kind='charge'),1)

    def test_surge_self_target_pays_then_restores_own_counter(self):
        c=self.case('Surge Node','target_self_0')
        self.assertIn(dict(op='assert_counter',ref='source',counter_kind='charge',value=5),self.pending(c))
        self.assertEqual(self.last(c,'assert_counter',ref='source',counter_kind='charge'),6)

    def test_six_real_spends_and_six_real_untaps_exhaust_counter_cost(self):
        c=self.case('Surge Node','spend_all_1');s=c['steps']
        self.assertEqual(sum(x['op']=='activate' and x['ref']=='source' and x['value']==1 for x in s),6)
        self.assertEqual(sum(x['op']=='cast' and x['ref'].startswith('untap_') for x in s),6)
        self.assertEqual(self.last(c,'assert_counter',ref='source',counter_kind='charge'),0)
        self.assertEqual(self.last(c,'assert_counter',ref='native',counter_kind='charge'),6)
        self.assertEqual(self.last(c,'require_card',ref='source',property='tapped'),0)
        self.assertEqual(self.last(c,'require_can_activate',ref='source'),0)
        self.assertEqual(self.last(c,'assert_mana_total',player=1),68)

    def test_chosen_x_is_paid_as_x_plus_one_and_applied_after_resolution(self):
        for owner in (0,1):
            for mode,x in [('x0',0),('x1',1),('x5',5)]:
                c=self.case('Ventifact Bottle',f'{mode}_{owner}')
                self.assertIn(dict(op='choose_x',player=owner,value=x),c['steps'])
                self.assertIn(dict(op='assert_counter',ref='source',counter_kind='charge',value=0),self.pending(c))
                self.assertEqual(self.last(c,'assert_counter',ref='source',counter_kind='charge'),x)
                self.assertEqual(self.last(c,'assert_mana_total',player=owner),79-x)

    def test_exact_printed_payments_have_zero_left(self):
        for nm in model.pins()['sources']:
            c=self.case(nm,'payment_0')
            self.assertEqual(self.last(c,'assert_mana_total',player=0),0)
            self.assertEqual([self.last(c,'assert_mana',player=0,color=i) for i in range(6)],[0]*6)

    def test_sorcery_only_is_oracle_permission_not_legacy_query_behavior(self):
        c=self.case('Ventifact Bottle','offturn_1')
        self.assertIn(dict(op='require_can_activate',ref='source',value=0),c['steps'])
        self.assertIn(dict(op='activate',ref='source',value=0),c['steps'])
        self.assertEqual(self.last(c,'assert_counter',ref='source',counter_kind='charge'),0)

    def test_core_tapper_fresh_tap_denial_does_not_deny_legal_self_sacrifice(self):
        for owner in (0,1):
            tap=self.case('Coretapper',f'fresh_{owner}',0);sac=self.case('Coretapper',f'fresh_{owner}',1)
            self.assertIn(dict(op='require_can_activate',ref='source',value=1),tap['steps'])
            self.assertIn(dict(op='choose_dialog',player=owner,value=0),tap['steps'])
            self.assertIn(dict(op='activate',ref='source',value=0),tap['steps'])
            self.assertIn(dict(op='activate',ref='source',value=1),sac['steps'])
            self.assertFalse(any(x['op']=='choose_target' for x in tap['steps']))

    def test_actual_cast_turn_gate_readies_core_tapper_without_counter_or_sickness_setter(self):
        c=self.case('Coretapper','charge_1',0)
        self.assertEqual(sum(x['op']=='advance_turn' for x in c['steps']),2)
        self.assertFalse(any(x['op']=='advance_turn' for x in self.case('Coretapper','fresh_1',0)['steps']))

    def test_tapped_core_tapper_query_includes_sacrifice_sibling_and_disabled_choice_is_attempted(self):
        c=self.case('Coretapper','tapped_1',0)
        self.assertIn(dict(op='require_can_activate',ref='source',value=1),c['steps'])
        self.assertIn(dict(op='choose_dialog',player=1,value=0),c['steps'])
        self.assertIn(dict(op='activate',ref='source',value=0),c['steps'])

    def test_culling_creature_cost_has_unique_own_victim_and_enemy_untouched(self):
        for owner in (0,1):
            c=self.case('Culling Dais',f'charge_{owner}')
            self.assertIn(dict(op='require_card',ref='enemy_victim',property='in_play',value=1),self.pending(c))
            self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='graveyard',index=self.rows['Sage of Lat-Nam']['csvid'],value=1),self.pending(c))
            self.assertEqual([x['ref'] for x in c['steps'] if x['op']=='choose_target'],['victim'] if owner==0 else [])

    def test_culling_zero_printed_cost_budget_keeps_draw_sibling_unaffordable(self):
        for mode in ('tapped','no_own_victim','repeat'):
            c=self.case('Culling Dais',mode+'_0')
            budget=[x['value'] for x in c['steps'] if x['op']=='set_mana' and x['player']==0]
            self.assertEqual(budget[6:],[0]*6)
            self.assertIn(dict(op='require_can_activate',ref='source',value=0),c['steps'])

    def test_battery_shortage_keeps_oracle_legal_mana_sibling_global_query(self):
        c=self.case('Black Mana Battery','insufficient_mana_0')
        self.assertIn(dict(op='require_can_activate',ref='source',value=1),c['steps'])
        self.assertFalse(any(x['op']=='activate' for x in c['steps']))
        self.assertEqual(self.last(c,'assert_counter',ref='source',counter_kind='charge'),0)
        self.assertEqual(self.last(c,'assert_mana_total',player=0),0)

    def test_trigon_wrong_color_has_no_creature_target_for_its_other_mode(self):
        c=self.case('Trigon of Corruption','wrong_color_1')
        self.assertFalse(any('Creature' in self.oracle[x['name']]['type_line'] for x in c['setup'] if x['zone']=='battlefield'))
        self.assertIn(dict(op='require_can_activate',ref='source',value=0),c['steps'])
        self.assertEqual(self.last(c,'assert_counter',ref='source',counter_kind='charge'),3)

    def test_any_artifact_means_enemy_and_nonnative_and_self_not_just_charge_users(self):
        for nm,ability in [('Coretapper',0),('Coretapper',1),('Surge Node',1)]:
            enemy=self.case(nm,'target_enemy_1',ability)
            self.assertIn(dict(op='choose_target',player=1,ref='enemy_native'),enemy['steps'])
            ordinary=self.case(nm,'target_nonnative_1',ability);steps=ordinary['steps']
            probe=next(i for i,x in enumerate(steps) if x['op']=='probe_activation_target')
            self.assertEqual(steps[probe]['value'],1);self.assertEqual(steps[probe]['other'],'nonnative')
            self.assertLess(probe,next(i for i,x in enumerate(steps) if x['op']=='activate'))

    def test_nonartifact_probe_is_real_and_has_no_paid_activation(self):
        c=self.case('Surge Node','probe_nonartifact_0')
        self.assertIn(dict(op='probe_activation_target',ref='source',other='invalid',index=-1,value=0),c['steps'])
        self.assertFalse(any(x['op']=='activate' for x in c['steps']))
        self.assertEqual(self.last(c,'assert_counter',ref='source',counter_kind='charge'),6)

    def test_self_sac_targeting_self_leaves_no_live_target_or_ghost_receipt(self):
        c=self.case('Coretapper','target_self_0',1)
        self.assertEqual(self.last(c,'assert_zone_multiplicity',player=0,zone='battlefield',index=self.rows['Coretapper']['csvid']),0)
        activated=next(i for i,x in enumerate(c['steps']) if x['op']=='activate')
        self.assertFalse(any(x['op']=='assert_counter' and x['ref']=='source' for x in c['steps'][activated:]))

    def test_removed_target_does_not_refund_source_counter_cost(self):
        c=self.case('Surge Node','target_removed_pending_1')
        self.assertEqual(self.last(c,'assert_counter',ref='source',counter_kind='charge'),5)
        self.assertEqual(self.last(c,'assert_zone_multiplicity',player=1,zone='battlefield',index=self.rows['Lux Cannon']['csvid']),0)

    def test_source_removal_keeps_other_target_addition_but_self_addition_cannot_find_source(self):
        c=self.case('Surge Node','source_removed_pending_0')
        self.assertEqual(self.last(c,'assert_counter',ref='native',counter_kind='charge'),1)
        c=self.case('Trigon of Corruption','source_removed_pending_0')
        self.assertEqual(self.last(c,'assert_zone_multiplicity',player=0,zone='battlefield',index=self.rows['Trigon of Corruption']['csvid']),0)

    def test_real_untap_allows_second_cost_and_keeps_paid_charge_history(self):
        c=self.case('Surge Node','repeat_untap_1')
        self.assertEqual(self.last(c,'assert_counter',ref='source',counter_kind='charge'),4)
        self.assertEqual(self.last(c,'assert_counter',ref='native',counter_kind='charge'),2)
        self.assertEqual(self.last(c,'assert_mana_total',player=1),77)

    def test_snapshot_replays_the_correct_action_boundary(self):
        for mode,n in [('ready_snapshot',2),('pending_snapshot',1),('resolved_snapshot',1)]:
            c=self.case('Surge Node',mode+'_0')
            self.assertEqual(sum(x['op']=='activate' for x in c['steps']),n)
            for op in ('capture','restore','ai_roundtrip','assert_recorded_state'):
                self.assertEqual(sum(x['op']==op for x in c['steps']),1)

    def test_unique_nonnmana_intent_is_input_receipt_immediate_after_activation(self):
        for nm in ('Gemstone Array',"Mirrodin's Core"):
            c=self.case(nm,'charge_1');s=c['steps'];a=next(i for i,x in enumerate(s) if x['op']=='activate')
            self.assertEqual(s[a+1],dict(op='require_activation_mode',value=1))
            intent=next(x for x in s if x['op']=='choose_activation_mode')
            self.assertEqual(intent['mode_label'],'Add charge counter');self.assertTrue(intent['allow_auto_nonmana'])

    def test_no_first_main_conversion_is_synthesized(self):
        c=self.case('Ventifact Bottle','cleanup_0')
        self.assertFalse(any(x['op'] in ('advance_turn','begin_main','first_main') for x in c['steps']))
        self.assertEqual(self.last(c,'assert_counter',ref='source',counter_kind='charge'),3)
        self.assertTrue(any('At the beginning of your first main phase' in x for x in c['_recipe']['untested_components']))

    def test_predeclared_untap_spell_is_in_pending_hand_receipt(self):
        c=self.case('Surge Node','repeat_untap_0')
        self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='hand',index=self.rows['Twiddle']['csvid'],value=1),self.pending(c))

    def test_charge_on_creature_never_uses_generic_plus_one_stats(self):
        c=self.case('Coretapper','target_self_0',0)
        self.assertEqual(self.last(c,'assert_counter',ref='source',counter_kind='charge'),1)
        self.assertEqual(self.last(c,'require_card',ref='source',property='power'),1)
        self.assertEqual(self.last(c,'require_card',ref='source',property='toughness'),1)

    def test_family_version_variants_and_whole_text_fail_closed(self):
        nm='Surge Node'
        for key,value in [('family','permanent_value_activation'),('version',True),('version',1),('variants',[]),('variants',['charge_0','charge_0']),('variants',['bogus_0']),('variants',[1]),('oracle_text','Put a charge counter on target artifact.')]:
            d=self.declaration(nm);d[key]=value
            with self.subTest(key=key,value=value),self.assertRaises(ValueError):model.spec(d,self.oracle[nm])

    def test_oracle_clauses_and_interface_cannot_be_forged(self):
        for change in ('gap','ui','ability'):
            d=self.declaration('Coretapper')
            if change=='gap':d['params']['inactive_clauses']=[]
            if change=='ui':d['params']['interface']['dialogs']['1']=1
            if change=='ability':d['params']['ability']=True
            with self.subTest(change=change),self.assertRaises(ValueError):model.spec(d,self.oracle['Coretapper'])

    def test_witness_pin_changes_are_rejected(self):
        changed=copy.deepcopy(self.oracle);changed['Sage of Lat-Nam']['power']='2'
        with self.assertRaises(ValueError):model.validate_witnesses(changed,self.rows)

    def test_remaining_colored_batteries_keep_exact_two_cost_and_charge_not_mana(self):
        for nm in ('Blue Mana Battery','Green Mana Battery','Red Mana Battery','White Mana Battery'):
            for owner in (0,1):
                c=self.case(nm,f'payment_{owner}')
                self.assertIn(dict(op='choose_dialog',player=owner,value=1),c['steps'])
                self.assertEqual(self.last(c,'assert_counter',ref='source',counter_kind='charge'),1)
                self.assertEqual(self.last(c,'assert_mana_total',player=owner),0)

    def test_coalition_selected_charge_does_not_fake_mandatory_first_main_conversion(self):
        c=self.case('Coalition Relic','pending_snapshot_1');s=c['steps']
        self.assertIn(dict(op='choose_dialog',player=1,value=1),s)
        self.assertIn(dict(op='assert_counter',ref='source',counter_kind='charge',value=0),self.pending(c))
        self.assertEqual(self.last(c,'assert_counter',ref='source',counter_kind='charge'),1)
        self.assertEqual(self.last(c,'assert_mana_total',player=1),80)
        self.assertFalse(any(x['op'] in ('advance_turn','begin_main','first_main') for x in s))
        self.assertTrue(any('At the beginning of your first main phase' in gap for gap in c['_recipe']['untested_components']))

    def test_new_numeric_three_and_four_payments_are_printed(self):
        for nm,cost in [('Titan Forge',3),("Magistrate's Scepter",4)]:
            c=self.case(nm,'payment_0');s=c['steps'];activated=next(i for i,x in enumerate(s) if x['op']=='activate')
            budgets=[x['value'] for x in s[:activated] if x['op']=='set_mana' and x['player']==0]
            self.assertEqual(budgets[6:],[cost,0,0,0,0,0])
            self.assertEqual(self.last(c,'assert_mana_total',player=0),0)
            self.assertEqual(self.last(c,'assert_counter',ref='source',counter_kind='charge'),1)

    def test_lux_source_alias_witness_survives_with_independent_zero_counters(self):
        c=self.case('Lux Cannon','source_removed_pending_0')
        self.assertEqual(self.last(c,'assert_zone_multiplicity',player=0,zone='battlefield',index=self.rows['Lux Cannon']['csvid']),1)
        self.assertEqual(self.last(c,'assert_counter',ref='native',counter_kind='charge'),0)
        self.assertEqual(self.last(c,'assert_zone_multiplicity',player=0,zone='graveyard',index=self.rows['Lux Cannon']['csvid']),1)

    def test_atlas_grindclock_and_ratchet_selected_charge_leaves_siblings_unfired(self):
        for nm in ('Otherworld Atlas','Grindclock','Ratchet Bomb'):
            c=self.case(nm,'charge_1')
            self.assertIn(dict(op='choose_dialog',player=1,value=0),c['steps'])
            self.assertEqual(self.last(c,'assert_counter',ref='source',counter_kind='charge'),1)
            self.assertEqual(self.last(c,'assert_zone',player=1,zone='hand'),1)
            self.assertEqual(self.last(c,'assert_zone',player=1,zone='library'),1)

    def test_trigon_rage_cast_cost_is_two_and_charge_cost_is_two_red(self):
        c=self.case('Trigon of Rage','payment_1');s=c['steps'];cast=next(i for i,x in enumerate(s) if x['op']=='cast')
        self.assertEqual([x['value'] for x in s[:cast] if x['op']=='set_mana'],[2,0,0,0,0,0])
        a=next(i for i,x in enumerate(s) if x['op']=='activate')
        self.assertEqual([x['value'] for x in s[cast:a] if x['op']=='set_mana' and x['player']==1],[0,0,0,0,2,0])
        self.assertEqual(self.last(c,'assert_counter',ref='source',counter_kind='charge'),4)

    def test_kyren_zero_chosen_x_mana_sibling_is_legal_with_no_charge_counters(self):
        source=model.pins()['sources']['Kyren Toy'];domain=source['abilities']['0']['mana_sibling_x_domain']
        self.assertEqual(domain,dict(minimum=0,maximum='available_charge_counters',zero_charge_cost=0,zero_mana_result=1))
        self.assertIn('{T}, Remove X charge counters from this artifact: Add an amount of {C} equal to X plus one.',source['oracle']['oracle_text'])
        for owner in (0,1):
            c=self.case('Kyren Toy',f'insufficient_mana_{owner}')
            self.assertIn(dict(op='require_can_activate',ref='source',value=1),c['steps'])
            self.assertFalse(any(x['op']=='activate' for x in c['steps']))
            self.assertEqual(self.last(c,'assert_counter',ref='source',counter_kind='charge'),0)
            self.assertEqual(self.last(c,'assert_mana_total',player=owner),0)

    def test_kyren_main_charge_cost_is_one_not_the_colored_battery_two(self):
        for owner in (0,1):
            c=self.case('Kyren Toy',f'payment_{owner}');s=c['steps'];a=next(i for i,x in enumerate(s) if x['op']=='activate')
            self.assertEqual([x['value'] for x in s[:a] if x['op']=='set_mana' and x['player']==owner][6:],[1,0,0,0,0,0])
            self.assertEqual(self.last(c,'assert_mana_total',player=owner),0)
            self.assertEqual(self.last(c,'assert_counter',ref='source',counter_kind='charge'),1)

    def test_infestation_wrong_color_keeps_legal_token_sibling_and_attempts_disabled_charge(self):
        for owner in (0,1):
            c=self.case('Trigon of Infestation',f'wrong_color_{owner}');s=c['steps']
            self.assertIn(dict(op='require_can_activate',ref='source',value=1),s)
            self.assertIn(dict(op='choose_dialog',player=owner,value=1),s)
            self.assertIn(dict(op='activate',ref='source',value=0),s)
            self.assertFalse(any(x['op']=='choose_target' for x in s))
            self.assertEqual(self.last(c,'assert_counter',ref='source',counter_kind='charge'),3)
            self.assertEqual(self.last(c,'require_card',ref='source',property='tapped'),0)
            self.assertEqual(self.last(c,'assert_mana_total',player=owner),2)
            self.assertEqual(self.last(c,'assert_zone',player=owner,zone='battlefield'),4)

    def test_new_schema_domains_reject_boolean_or_unbounded_x_even_if_interface_pin_matches(self):
        for key,value in [('minimum',False),('minimum',-1),('maximum','unbounded'),('zero_charge_cost',1),('zero_mana_result',0)]:
            data=copy.deepcopy(model.pins());ui=data['sources']['Kyren Toy']['abilities']['0'];ui['mana_sibling_x_domain'][key]=value
            d=self.declaration('Kyren Toy');d['params']['interface']=copy.deepcopy(ui)
            with self.subTest(key=key,value=value),patch.object(model,'pins',return_value=data),self.assertRaises(ValueError):
                model.spec(d,self.oracle['Kyren Toy'])

    def test_disabled_cost_mode_schema_is_scoped_to_equal_total_colored_cost_with_legal_untargeted_sibling(self):
        data=copy.deepcopy(model.pins());ui=data['sources']['Trigon of Infestation']['abilities']['1'];ui['disabled_cost_modes']=['tapped']
        d=self.declaration('Trigon of Infestation');d['params']['interface']=copy.deepcopy(ui)
        with patch.object(model,'pins',return_value=data),self.assertRaises(ValueError):model.spec(d,self.oracle['Trigon of Infestation'])


if __name__=='__main__':
    unittest.main()
