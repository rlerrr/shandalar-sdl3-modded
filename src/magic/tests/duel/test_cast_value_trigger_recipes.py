import copy
import json
import unittest
from pathlib import Path

from generate_cases import registry
import cast_value_trigger_recipes as f


class CastValueTriggerTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows={r['name']:r for r in registry()}

    def declaration(self,name,variant='effect_0'):
        p=f.classify(self.oracle[name])
        d=dict(family=f.FAMILY,version=2,oracle_text=self.oracle[name]['oracle_text'],
               params=f.parameters(p),variants=[variant])
        return d

    def case(self,name,variant='effect_0'):
        d=self.declaration(name,variant);p=f.spec(d,self.oracle[name])
        return f.expand(name,self.rows[name]['csvid'],d,p,d['variants'],self.rows,self.oracle)[0]

    def final_value(self,steps,op,**fields):
        return next(s['value'] for s in reversed(steps) if s['op']==op and all(s.get(k)==v for k,v in fields.items()))

    def test_whole_source_identity_and_explicit_parameters_fail_closed(self):
        for name in f.pins()['sources']:
            d=self.declaration(name)
            for key,value in [('oracle_id','other'),('mana_cost','{10}'),('layout','transform'),('oracle_text','Whenever you cast a spell, draw a card.')]:
                r=copy.deepcopy(self.oracle[name]);r[key]=value
                with self.assertRaises(ValueError):f.spec(d,r)
            for key,value in [('scope','any'),('amount',True),('optional',0),('unknown',1)]:
                changed=copy.deepcopy(d);changed['params'][key]=value
                if changed['params']==d['params'] and type(value) is type(d['params'].get(key)):continue
                with self.assertRaises(ValueError):f.spec(changed,self.oracle[name])

    def test_full_text_grammar_rejects_hidden_additional_effects(self):
        for text in ['Whenever you cast your third spell each turn, draw a card.',
                     'Whenever you cast a creature spell, draw a card. You gain 2 life.',
                     'Whenever you cast a spell that targets this creature, draw a card.',
                     'Whenever you cast a spell, draw a card.\nCreatures you control get +1/+1.']:
            r=copy.deepcopy(self.oracle['Contemplation']);r['oracle_text']=text
            with self.assertRaises(ValueError):f.classify(r)

    def test_all_independent_witness_fields_and_registry_identities_are_pinned(self):
        f.validate_witnesses(self.oracle,self.rows)
        for name,key,value in [('Grizzly Bears','power','3'),('Sol Ring','mana_cost','{0}'),('Seal of Cleansing','type_line','Artifact'),('Counterspell','oracle_text','Counter target creature spell.')]:
            altered=copy.deepcopy(self.oracle);altered[name][key]=value
            with self.assertRaises(ValueError):f.validate_witnesses(altered,self.rows)

    def test_predicates_distinguish_overlapping_types_and_colors(self):
        tests=[('Ornithopter','artifact',True),('Ornithopter','creature',True),('Ornithopter','noncreature',False),
               ('Sol Ring','noncreature',True),('Grizzly Bears','artifact',False),
               ('Watchwolf','multicolored',True),('Watchwolf','white',True),('Grizzly Bears','multicolored',False),
               ('Sol Ring','colorless',True),('Scathe Zombies','nonblack',False)]
        for name,predicate,expected in tests:self.assertEqual(f.matches(self.oracle[name],predicate),expected)

    def test_positive_effects_are_independent_of_the_triggering_spell_effect(self):
        for owner in (0,1):
            steps=self.case('Contemplation',f'effect_{owner}')['steps']
            self.assertEqual(self.final_value(steps,'assert_life',player=owner),21)
            steps=self.case('Ebon Drake',f'effect_{owner}')['steps']
            self.assertEqual(self.final_value(steps,'assert_life',player=owner),19)
            steps=self.case('Beast Whisperer',f'effect_{owner}')['steps']
            initial=[s for s in self.case('Beast Whisperer',f'effect_{owner}')['setup'] if s['zone']=='library' and s['player']==owner]
            self.assertEqual(self.final_value(steps,'assert_zone',player=owner,zone='library'),len(initial)-1)

    def test_scope_controls_do_not_change_source_owner(self):
        for owner in (0,1):
            own=self.case('Contemplation',f'opponent_cast_{owner}')['steps']
            self.assertEqual(self.final_value(own,'assert_life',player=owner),20)
            any_caster=self.case('Ebon Drake',f'opponent_cast_{owner}')['steps']
            self.assertEqual(self.final_value(any_caster,'assert_life',player=owner),19)
            opponent=self.case('Memory Erosion',f'opponent_cast_{owner}')['steps']
            before=len([s for s in self.case('Memory Erosion',f'opponent_cast_{owner}')['setup'] if s['zone']=='library' and s['player']==owner])
            self.assertEqual(self.final_value(opponent,'assert_zone',player=owner,zone='library'),before)

    def test_type_filter_has_a_real_paid_nonmatching_cast(self):
        for name in ("Enchantress's Presence",'Vedalken Archmage','Beast Whisperer'):
            case=self.case(name,'nonmatching_spell_0');steps=case['steps']
            self.assertIn(dict(op='cast',ref='spell_0',value=1),steps)
            before=sum(s['zone']=='library' and s['player']==0 for s in case['setup'])
            self.assertEqual(self.final_value(steps,'assert_zone',player=0,zone='library'),before)

    def test_source_removed_before_and_arriving_after_do_not_see_earlier_casts(self):
        for name in ('Contemplation','Ebon Drake','Beast Whisperer'):
            for variant in ('source_removed_before_0','source_arrives_late_0'):
                case=self.case(name,variant);steps=case['steps']
                self.assertEqual(self.final_value(steps,'assert_life',player=0),20)
                before=sum(s['zone']=='library' and s['player']==0 for s in case['setup'])
                self.assertEqual(self.final_value(steps,'assert_zone',player=0,zone='library'),before)

    def test_removal_after_cast_does_not_erase_already_earned_value(self):
        steps=self.case('Contemplation','source_removed_after_cast_0')['steps']
        announce=next(i for i,s in enumerate(steps) if s['op']=='announce_spell')
        destroy=next(i for i,s in enumerate(steps) if s['op']=='destroy')
        self.assertLess(announce,destroy)
        self.assertEqual(self.final_value(steps,'assert_life',player=0),21)

    def test_countered_spell_still_triggers_and_counterspell_is_a_second_cast(self):
        for owner in (0,1):
            steps=self.case('Ebon Drake',f'countered_trigger_{owner}')['steps']
            self.assertEqual(self.final_value(steps,'assert_life',player=owner),18)
            self.assertLess(next(i for i,s in enumerate(steps) if s['op']=='announce_spell'),next(i for i,s in enumerate(steps) if s['op']=='cast_response'))
            self.assertIn(dict(op='assert_stack',value=2),steps)
            self.assertIn(dict(op='resolve_response'),steps)
            self.assertEqual(self.final_value(steps,'assert_zone_multiplicity',player=owner,zone='graveyard',index=self.rows['Sol Ring']['csvid']),2)

    def test_optional_choices_accept_or_decline_the_trigger_itself(self):
        for name in ("Dragon's Claw",'Primordial Sage'):
            accept=self.case(name)['steps'];decline=self.case(name,'decline_0')['steps']
            self.assertIn(dict(op='choose_trigger',player=0,ref='source'),accept)
            self.assertFalse(any(s['op']=='choose_trigger' for s in decline))
            self.assertIn(dict(op='assert_choices_consumed'),decline)
        self.assertEqual(self.final_value(self.case("Dragon's Claw",'decline_0')['steps'],'assert_life',player=0),20)

    def test_real_source_and_witness_payments_have_separate_receipts(self):
        steps=self.case('Contemplation','source_payment_0')['steps']
        cast=next(i for i,s in enumerate(steps) if s['op']=='cast')
        restore=next(i for i,s in enumerate(steps) if i>cast and s['op']=='set_mana' and s['value']==20)
        self.assertTrue(all(dict(op='assert_mana',player=0,color=c,value=0) in steps[:restore] for c in range(6)))
        steps=self.case('Beast Whisperer','trigger_payment_0')['steps']
        self.assertTrue(all(self.final_value(steps,'assert_mana',player=0,color=c)==0 for c in range(6)))

    def test_pending_spells_use_their_real_hand_state_and_all_snapshots_replay(self):
        for name in f.pins()['sources']:
            for position in ('ready_snapshot','pending_snapshot','resolved_snapshot'):
                steps=self.case(name,position+'_1')['steps']
                self.assertIn(dict(op='ai_roundtrip'),steps);self.assertEqual(steps[-1],dict(op='assert_recorded_state'))
            self.assertIn(dict(op='assert_card',ref='spell_0',property='in_hand',value=0),self.case(name)['steps'])

    def test_mill_empty_and_short_libraries_are_bounded_and_do_not_draw(self):
        for owner in (0,1):
            for variant,amount in [('short_library',1),('empty_library',0)]:
                case=self.case('Memory Erosion',f'{variant}_{owner}');steps=case['steps'];target=1-owner
                self.assertEqual(self.final_value(steps,'assert_zone',player=target,zone='library'),0)
                before=sum(s['zone']=='graveyard' and s['player']==target for s in case['setup'])
                self.assertEqual(self.final_value(steps,'assert_zone',player=target,zone='graveyard'),before+amount)

    def test_repeated_casts_stay_on_the_same_turn_and_accumulate(self):
        for owner in (0,1):
            steps=self.case('Contemplation',f'repeat_same_turn_{owner}')['steps']
            self.assertEqual(sum(s['op']=='cast' for s in steps),3)
            self.assertFalse(any(s['op']=='advance_turn' for s in steps))
            self.assertEqual(self.final_value(steps,'assert_life',player=owner),23)

    def test_declared_inputs_and_oracle_records_are_immutable(self):
        d=self.declaration('Contemplation');before=copy.deepcopy(d);record=copy.deepcopy(self.oracle['Contemplation'])
        p=f.spec(d,record);f.expand('Contemplation',3351,d,p,d['variants'],self.rows,self.oracle)
        self.assertEqual(d,before);self.assertEqual(record,self.oracle['Contemplation'])

    def test_cleanup_checks_paid_pools_before_explicit_emptying_and_preserves_value(self):
        for owner in (0,1):
            case=self.case('Contemplation',f'cleanup_{owner}');steps=case['steps']
            cleanup=next(i for i,s in enumerate(steps) if s['op']=='cleanup')
            emptied=steps[cleanup-12:cleanup]
            self.assertEqual(emptied,[dict(op='set_mana',player=who,color=color,value=0) for who in (0,1) for color in range(6)])
            self.assertTrue(all(s['op']=='assert_mana_total' and s['value']>0 for s in steps[cleanup-14:cleanup-12]))
            self.assertEqual(self.final_value(steps,'assert_life',player=owner),21)
            self.assertTrue(any('outside the cleanup helper' in gap for gap in case['_recipe']['untested_components']))

    def test_seeded_zone_indexes_are_never_inspected_as_live_instance_refs(self):
        for name in f.pins()['sources']:
            for mode in ('effect','source_removed_before','source_removed_after_cast','source_arrives_late'):
                case=self.case(name,mode+'_0')
                zone_refs={s['ref'] for s in case['setup'] if s['zone'] in ('graveyard','library','exile')}
                self.assertFalse(any(s['op'] in ('assert_card','require_card') and s.get('ref') in zone_refs for s in case['steps']))

    def test_paid_value_cost_and_effect_are_derived_from_the_complete_clause(self):
        for name,amount,effect in [('Crystal Rod',1,'gain'),('Lifesmith',3,'gain'),('Lunar Mystic',1,'draw')]:
            p=f.classify(self.oracle[name])
            self.assertEqual((p['trigger_cost'],p['amount'],p['effect']),('{1}',amount,effect))
            for key,value in [('trigger_cost','{0}'),('amount',amount+1),('mechanism','cast_stats')]:
                d=self.declaration(name);d['params'][key]=value
                with self.assertRaises(ValueError):f.spec(d,self.oracle[name])

    def test_paid_casts_pay_parent_and_trigger_separately_and_allow_decline(self):
        for name,gain in [('Crystal Rod',1),('Lifesmith',3)]:
            for owner in (0,1):
                steps=self.case(name,f'trigger_payment_{owner}')['steps']
                self.assertEqual(self.final_value(steps,'assert_mana_total',player=owner),0)
                self.assertEqual(self.final_value(steps,'assert_life',player=owner),20+gain)
            self.assertEqual(self.final_value(self.case(name,'decline_0')['steps'],'assert_life',player=0),20)

    def test_unaffordable_paid_trigger_does_not_produce_free_value(self):
        for name in ('Crystal Rod','Lifesmith','Lunar Mystic'):
            for owner in (0,1):
                case=self.case(name,f'trigger_unaffordable_{owner}');steps=case['steps']
                self.assertEqual(self.final_value(steps,'assert_mana_total',player=owner),0)
                self.assertEqual(self.final_value(steps,'assert_life',player=owner),20)
                before=sum(s['zone']=='library' and s['player']==owner for s in case['setup'])
                self.assertEqual(self.final_value(steps,'assert_zone',player=owner,zone='library'),before)

    def test_instant_only_watchers_do_not_charge_or_trigger_for_sorceries(self):
        for owner in (0,1):
            case=self.case('Lunar Mystic',f'sorcery_control_{owner}');steps=case['steps']
            before=sum(s['zone']=='library' and s['player']==owner for s in case['setup'])
            self.assertEqual(self.final_value(steps,'assert_zone',player=owner,zone='library'),before-2)
            self.assertEqual(self.final_value(steps,'assert_mana_total',player=owner),117)

    def test_heroic_requires_this_source_to_be_targeted_by_its_controller(self):
        csvid=self.rows['Satyr Hoplite']['csvid']
        for owner in (0,1):
            own=self.case('Satyr Hoplite',f'effect_{owner}')['steps']
            self.assertIn(dict(op='choose_target',player=owner,ref='source'),own)
            self.assertEqual(self.final_value(own,'assert_card',ref='source',property='power'),5)
            self.assertEqual(self.final_value(own,'assert_card',ref='source',property='plus1_counters'),1)
            other=self.case('Satyr Hoplite',f'other_target_{owner}')['steps']
            self.assertEqual(self.final_value(other,'assert_card',ref='source',property='plus1_counters'),0)
            opposing=self.case('Satyr Hoplite',f'opponent_cast_{owner}')['steps']
            self.assertEqual(self.final_value(opposing,'assert_card',ref='source',property='power'),4)
            self.assertEqual(self.final_value(opposing,'assert_card',ref='source',property='plus1_counters'),0)

    def test_cleanup_removes_temporary_stats_but_preserves_actual_counters(self):
        for name,expected,counters in [('Cunning Breezedancer',4,0),('Deeproot Champion',2,1),('Satyr Hoplite',2,1)]:
            for owner in (0,1):
                steps=self.case(name,f'cleanup_{owner}')['steps']
                self.assertEqual(self.final_value(steps,'assert_card',ref='source',property='power'),expected)
                self.assertEqual(self.final_value(steps,'assert_card',ref='source',property='plus1_counters'),counters)

    def test_negative_cast_pumps_cause_state_based_death_at_zero_toughness(self):
        for owner in (0,1):
            steps=self.case('Blind Creeper',f'repeat_same_turn_{owner}')['steps']
            self.assertEqual(self.final_value(steps,'assert_zone_multiplicity',player=owner,zone='graveyard',index=self.rows['Blind Creeper']['csvid']),1)
            self.assertEqual(self.final_value(steps,'assert_zone',player=owner,zone='battlefield'),5)
            self.assertFalse(any(s['op']=='destroy' for s in steps))

    def test_heroic_copies_keep_nonselected_same_name_sources_as_controls(self):
        steps=self.case('Satyr Hoplite','two_sources_0')['steps']
        self.assertEqual(self.final_value(steps,'assert_card',ref='source',property='plus1_counters'),1)
        self.assertEqual(self.final_value(steps,'assert_card',ref='copy',property='plus1_counters'),0)

    def test_mana_denied_targeted_casts_do_not_queue_unreachable_target_choices(self):
        for name in ('Lunar Mystic','Triton Fortune Hunter','Satyr Hoplite'):
            for owner in (0,1):
                steps=self.case(name,f'trigger_insufficient_mana_{owner}')['steps']
                self.assertIn(dict(op='cast',ref='spell_0',value=0),steps)
                self.assertFalse(any(s['op'] in ('choose_target','choose_trigger') for s in steps))

    def test_cross_controller_artifact_trigger_pins_offer_without_weakening_life(self):
        for owner in (0,1):
            steps=self.case("Golem's Heart",f'opponent_cast_{owner}')['steps']
            resolve=next(i for i,s in enumerate(steps) if s['op']=='resolve')
            self.assertEqual(steps[resolve+1],dict(op='assert_unused_choices',value=0))
            self.assertEqual(self.final_value(steps,'assert_life',player=owner),21)
            if owner==0:self.assertIn(dict(op='choose_trigger',player=0,ref='source'),steps)

    def test_payment_presentation_does_not_impose_an_optional_outer_trigger(self):
        for mode in ('effect_0','opponent_cast_0','countered_trigger_0'):
            steps=self.case("Urza's Chalice",mode)['steps']
            self.assertFalse(any(s['op'] in ('choose_trigger','assert_unused_choices') for s in steps))
            if mode=='countered_trigger_0':
                self.assertLess(next(i for i,s in enumerate(steps) if s['op']=='announce_spell'),next(i for i,s in enumerate(steps) if s['op']=='cast_response'))
            self.assertEqual(self.final_value(steps,'assert_life',player=0),21)

    def test_paid_alternate_artifact_decline_keeps_a_real_artifact_creature_control(self):
        case=self.case("Urza's Chalice",'alternate_decline_0');steps=case['steps']
        self.assertEqual(next(s['name'] for s in case['setup'] if s['ref']=='spell_0'),'Ornithopter')
        self.assertFalse(any(s['op']=='choose_trigger' for s in steps))
        self.assertIn(dict(op='choose_cancel_mana',player=0),steps)
        self.assertEqual(self.final_value(steps,'assert_life',player=0),20)
        self.assertEqual(self.final_value(steps,'assert_mana_total',player=0),0)

    def test_payment_decline_has_a_real_available_source_and_distinct_casters(self):
        for mode,actor in [('decline_0',0),('alternate_decline_0',1)]:
            case=self.case("Urza's Chalice",mode);steps=case['steps']
            self.assertEqual(next(s['player'] for s in case['setup'] if s['ref']=='spell_0'),actor)
            land=next(s for s in case['setup'] if s['ref']=='optional_payment_land')
            self.assertEqual((land['name'],land['player'],land['zone']),('Forest',0,'battlefield'))
            self.assertNotIn('tapped',land.get('state',[]))
            resolve=next(i for i,s in enumerate(steps) if s['op']=='resolve')
            self.assertEqual(steps[resolve-1],dict(op='choose_cancel_mana',player=0))
            self.assertEqual(steps[resolve+1],dict(op='assert_unused_choices',value=0))
            self.assertEqual(self.final_value(steps,'assert_card',ref='optional_payment_land',property='tapped'),0)

    def test_payment_presentation_is_source_handler_and_witness_pinned(self):
        d=self.declaration("Urza's Chalice");d['presentation']={'acceptance':'trigger'}
        with self.assertRaises(ValueError):f.spec(d,self.oracle["Urza's Chalice"])
        rows=copy.deepcopy(self.rows);rows["Urza's Chalice"]['handler']='card_other'
        with self.assertRaises(ValueError):f.validate_witnesses(self.oracle,rows)

    def test_heroic_life_and_any_caster_counter_profiles_keep_independent_receipts(self):
        for owner in (0,1):
            life=self.case('Setessan Battle Priest',f'effect_{owner}')['steps']
            self.assertEqual(self.final_value(life,'assert_life',player=owner),22)
            negative=self.case('Setessan Battle Priest',f'other_target_{owner}')['steps']
            self.assertEqual(self.final_value(negative,'assert_life',player=owner),20)
            countered=self.case('Managorger Hydra',f'countered_trigger_{owner}')['steps']
            self.assertEqual(self.final_value(countered,'assert_card',ref='source',property='plus1_counters'),2)

    def library_delta(self,name,variant,player):
        c=self.case(name,variant)
        before=sum(x['zone']=='library' and x['player']==player for x in c['setup'])
        return before-self.final_value(c['steps'],'assert_zone',player=player,zone='library')

    def test_ordinal_first_second_third_are_real_announced_casts_with_independent_draws(self):
        for owner in (0,1):
            for mode,draws,casts in [('first_only',0,1),('effect',1,2),('third_spell',1,3),('source_cast',1,3),('source_arrives_late',0,3),('source_removed_after_cast',1,2)]:
                c=self.case('Jori En, Ruin Diver',f'{mode}_{owner}');s=c['steps']
                self.assertEqual(self.library_delta('Jori En, Ruin Diver',f'{mode}_{owner}',owner),draws)
                self.assertEqual(sum(x['op']=='cast' and x['value']==1 for x in s),casts)
                self.assertEqual(sum(x['op']=='announce_spell' for x in s),casts)
                self.assertFalse(any('history' in x['op'] for x in s))
            s=self.case('Jori En, Ruin Diver',f'third_spell_{owner}')['steps']
            values=[x['value'] for x in s if x['op']=='assert_zone' and x['player']==owner and x['zone']=='library']
            self.assertEqual(values,[28,27,27,27])

    def test_ordinal_reset_uses_real_turn_gates_and_checks_draw_on_each_new_second_spell(self):
        for owner in (0,1):
            s=self.case('Jori En, Ruin Diver',f'prior_turn_reset_{owner}')['steps']
            self.assertEqual(sum(x['op']=='advance_turn' for x in s),2)
            self.assertEqual(sum(x['op']=='turn' for x in s),1)
            self.assertEqual(self.library_delta('Jori En, Ruin Diver',f'prior_turn_reset_{owner}',owner),2)
            turns=[i for i,x in enumerate(s) if x['op']=='advance_turn']
            self.assertEqual(turns[1],turns[0]+1)
            self.assertEqual(s[turns[0]-12:turns[0]],[dict(op='set_mana',player=who,color=c,value=0) for who in (0,1) for c in range(6)])

    def test_ordinal_counts_countered_precursors_and_actual_response_casts_by_the_correct_player(self):
        for owner in (0,1):
            for mode in ('countered_precursor','countered_trigger','response_is_second','opponent_history'):
                self.assertEqual(self.library_delta('Jori En, Ruin Diver',f'{mode}_{owner}',owner),1)
            for mode in ('countered_precursor','countered_trigger','response_is_second'):
                s=self.case('Jori En, Ruin Diver',f'{mode}_{owner}')['steps']
                response=next(i for i,x in enumerate(s) if x['op']=='cast_response')
                self.assertTrue(any(x['op']=='announce_spell' for x in s[:response]))
                self.assertIn(dict(op='assert_stack',value=2),s)
                self.assertIn(dict(op='resolve_response'),s)
            self.assertEqual(self.library_delta('Jori En, Ruin Diver',f'opponent_history_{owner}',1-owner),1)

    def test_spirit_and_arcane_are_independent_branches_with_nonarcane_instant_controls(self):
        for owner in (0,1):
            for mode,witness in [('effect','Wandering Ones'),('alternate_spell','Reach Through Mists')]:
                c=self.case('Kami of the Hunt',f'{mode}_{owner}')
                self.assertEqual(next(x['name'] for x in c['setup'] if x['ref']=='spell_0'),witness)
                self.assertEqual(self.final_value(c['steps'],'assert_card',ref='source',property='power'),3)
                self.assertEqual(self.library_delta('Kami of the Hunt',f'{mode}_{owner}',owner),int(mode=='alternate_spell'))
            for mode in ('nonmatching_spell','nonmatching_instant','opponent_cast'):
                self.assertEqual(self.final_value(self.case('Kami of the Hunt',f'{mode}_{owner}')['steps'],'assert_card',ref='source',property='power'),2)
            s=self.case('Bounteous Kirin',f'alternate_spell_{owner}')['steps']
            self.assertEqual(self.final_value(s,'assert_life',player=owner),21)
            self.assertEqual(self.library_delta('Bounteous Kirin',f'alternate_spell_{owner}',owner),1)
            self.assertEqual(self.final_value(self.case('Bounteous Kirin',f'higher_value_{owner}')['steps'],'assert_life',player=owner),22)

    def test_printed_mana_values_scale_temporary_pumps_without_token_or_history_fabrication(self):
        for owner in (0,1):
            for mode,power in [('zero_value',1),('alternate_spell',2),('effect',3),('high_value',9),('mixed_values',4),('cleanup',1)]:
                c=self.case('Manaplasm',f'{mode}_{owner}')
                self.assertEqual(self.final_value(c['steps'],'assert_card',ref='source',property='power'),power)
                self.assertFalse(any('token' in x.get('state',[]) for x in c['setup']))

    def test_eldrazi_creature_and_mana_threshold_are_three_independent_conditions(self):
        for owner in (0,1):
            for mode,draws in [('below_threshold',0),('effect',2),('alternate_spell',2),('nonmatching_spell',0),('noncreature_subtype',0),('countered_trigger',2)]:
                self.assertEqual(self.library_delta("Emrakul's Influence",f'{mode}_{owner}',owner),draws)
            c=self.case("Emrakul's Influence",f'noncreature_subtype_{owner}')
            self.assertIn(dict(op='choose_target',player=owner,ref='idle'),c['steps'])
            self.assertEqual(self.final_value(c['steps'],'assert_card',ref='idle',property='power'),13)

    def test_aura_equipment_vehicle_union_has_all_three_positive_and_two_negative_receipts(self):
        for owner in (0,1):
            for mode,draws in [('effect',1),('alternate_spell',1),('vehicle_spell',1),('nonmatching_spell',0),('nonmatching_enchantment',0)]:
                self.assertEqual(self.library_delta('Sram, Senior Edificer',f'{mode}_{owner}',owner),draws)
            self.assertIn(dict(op='choose_target',player=owner,ref='idle'),self.case('Sram, Senior Edificer',f'alternate_spell_{owner}')['steps'])
            self.assertEqual(next(x['name'] for x in self.case('Sram, Senior Edificer',f'nonmatching_spell_{owner}')['setup'] if x['ref']=='spell_0'),'Sol Ring')

    def test_historic_and_legendary_supertype_are_distinct_and_overlapping_predicates_trigger_once(self):
        for owner in (0,1):
            for mode,draws in [('effect',1),('alternate_spell',1),('saga_spell',1),('overlap_once',1),('nonmatching_spell',0),('nonmatching_enchantment',0)]:
                self.assertEqual(self.library_delta('Jhoira, Weatherlight Captain',f'{mode}_{owner}',owner),draws)
            for mode,draws in [('effect',1),('alternate_spell',1),('legendary_artifact',1),('nonmatching_artifact',0),('nonmatching_spell',0)]:
                self.assertEqual(self.library_delta('Reki, the History of Kamigawa',f'{mode}_{owner}',owner),draws)
            for name in ('Jhoira, Weatherlight Captain','Reki, the History of Kamigawa'):
                self.assertEqual(self.library_delta(name,f'countered_trigger_{owner}',owner),1)

    def test_predicate_grammar_parameters_and_all_new_witness_fields_fail_closed(self):
        names=['Jori En, Ruin Diver','Kami of the Hunt','Bounteous Kirin','Manaplasm',"Emrakul's Influence",'Sram, Senior Edificer','Jhoira, Weatherlight Captain','Reki, the History of Kamigawa']
        for name in names:
            d=self.declaration(name)
            for key,value in [('ordinal',3),('reward_basis','handler'),('predicate_expr',{'op':'any'})]:
                changed=copy.deepcopy(d);changed['params'][key]=value
                if changed['params']==d['params']:continue
                with self.assertRaises(ValueError):f.spec(changed,self.oracle[name])
            r=copy.deepcopy(self.oracle[name]);r['oracle_text']+='\nCreatures you control get +1/+1.'
            with self.assertRaises(ValueError):f.classify(r)
        for name,key,value in [('Wandering Ones','type_line','Creature — Human'),('Reach Through Mists','type_line','Instant'),('Eldrazi Devastator','cmc',6),('Pacifism','type_line','Enchantment'),('Song of Freyalise','type_line','Enchantment'),('Isamaru, Hound of Konda','type_line','Creature — Dog')]:
            o=copy.deepcopy(self.oracle);o[name][key]=value
            with self.assertRaises(ValueError):f.validate_witnesses(o,self.rows)

    def test_damage_is_a_distinct_oracle_effect_with_complete_typed_source_grammar(self):
        for name in ('Pyrostatic Pillar',"Pyromancer's Assault",'Kaervek the Merciless','Lys Alana Bowmaster','Guttersnipe','Mindsparker','Pyroconvergence','Weaver of Lightning'):
            p=f.classify(self.oracle[name]);self.assertEqual((p['effect'],p['mechanism']),('damage','cast_damage'))
            r=copy.deepcopy(self.oracle[name]);r['oracle_text']+='\n{T}: You gain 1 life.'
            with self.assertRaises(ValueError):f.classify(r)
            d=self.declaration(name);d['params']['effect']='lose'
            with self.assertRaises(ValueError):f.spec(d,self.oracle[name])

    def test_damage_mana_bound_and_color_type_conjunction_have_independent_counterexamples(self):
        bound=f.classify(self.oracle['Pyrostatic Pillar'])['predicate_expr']
        for name,value in [('Ornithopter',True),('Scathe Zombies',True),('Hill Giant',False)]:self.assertEqual(f.damage_matches(self.oracle[name],bound),value)
        condition=f.classify(self.oracle['Mindsparker'])['predicate_expr']
        for name,value in [('Holy Day',True),('Reach Through Mists',True),('Divination',True),('Azorius Charm',True),('Coral Merfolk',False),('Giant Growth',False)]:self.assertEqual(f.damage_matches(self.oracle[name],condition),value)
        for owner in (0,1):
            for mode,life in [('effect',18),('zero_value',18),('above_threshold',20)]:
                self.assertEqual(self.final_value(self.case('Pyrostatic Pillar',f'{mode}_{owner}')['steps'],'assert_life',player=owner),life)

    def test_real_damage_receipts_distinguish_caster_opponents_and_counterspell_costs(self):
        for owner in (0,1):
            s=self.case('Pyrostatic Pillar',f'countered_trigger_{owner}')['steps']
            self.assertEqual(self.final_value(s,'assert_life',player=owner),18)
            self.assertEqual(self.final_value(s,'assert_life',player=1-owner),18)
            s=self.case('Guttersnipe',f'effect_{owner}')['steps']
            self.assertEqual(self.final_value(s,'assert_life',player=owner),20)
            self.assertEqual(self.final_value(s,'assert_life',player=1-owner),18)
            self.assertEqual(self.final_value(s,'assert_card',ref='idle',property='power'),6)

    def test_optional_flying_damage_accepts_real_source_and_target_or_declines_without_invented_damage(self):
        for owner in (0,1):
            c=self.case('Lys Alana Bowmaster',f'effect_{owner}');s=c['steps']
            self.assertEqual(next(x['name'] for x in c['setup'] if x['ref']=='spell_0'),'Elvish Mystic')
            self.assertEqual(self.final_value(s,'assert_card',ref='flyer_enemy',property='damage'),2)
            self.assertIn(dict(op='choose_target',player=owner,ref='flyer_enemy'),s)
            self.assertFalse(any(x['op']=='choose_trigger_source' for x in s))
            self.assertIn(dict(op='assert_unused_choices',value=0),s)
        s=self.case('Lys Alana Bowmaster','decline_0')['steps']
        self.assertEqual(self.final_value(s,'assert_card',ref='flyer_enemy',property='damage'),0)
        self.assertFalse(any(x['op']=='choose_trigger_source' for x in s))
        self.assertIn(dict(op='choose_optional_target',player=0,ref='flyer_enemy'),s)
        self.assertIn(dict(op='assert_unused_choices',value=0),s)
        self.assertEqual(self.final_value(s,'assert_forced_target_choices'),0)
        for owner in (0,1):
            s=self.case('Lys Alana Bowmaster',f'no_legal_target_{owner}')['steps']
            self.assertFalse(any(x['op'] in ('choose_trigger_source','choose_target','choose_optional_target') for x in s))

    def test_any_target_covers_both_players_creatures_and_real_printed_planeswalker_loyalty(self):
        for owner in (0,1):
            for mode,target_player in [('effect',1-owner),('own_player',owner)]:
                s=self.case('Pyroconvergence',f'{mode}_{owner}')['steps']
                self.assertEqual(self.final_value(s,'assert_life',player=target_player),18)
            for mode,ref in [('own_creature','idle'),('enemy_creature','enemy')]:
                s=self.case('Pyroconvergence',f'{mode}_{owner}')['steps']
                if ref=='idle':self.assertEqual(self.final_value(s,'assert_card',ref=ref,property='damage'),2)
                else:self.assertEqual(self.final_value(s,'assert_zone_multiplicity',player=1-owner,zone='graveyard',index=self.rows['Grizzly Bears']['csvid']),2)
            for mode in ('planeswalker','own_planeswalker'):
                c=self.case('Pyroconvergence',f'{mode}_{owner}');s=c['steps']
                self.assertEqual([x['value'] for x in s if x['op']=='assert_counter' and x.get('ref')=='walker'],[5,3,3])
                self.assertEqual(self.final_value(s,'assert_life',player=owner),20)
                self.assertEqual(self.final_value(s,'assert_life',player=1-owner),20)

    def test_lethal_creature_damage_is_verified_by_destination_without_stale_refs(self):
        for name,witness in [('Weaver of Lightning','Eager Cadet'),('Lys Alana Bowmaster','Suntail Hawk')]:
            for owner in (0,1):
                s=self.case(name,f'lethal_creature_{owner}')['steps']
                self.assertEqual(self.final_value(s,'assert_zone_multiplicity',player=1-owner,zone='graveyard',index=self.rows[witness]['csvid']),1)
                self.assertFalse(any(x['op']=='assert_card' and x.get('ref')=='lethal' for x in s))
        s=self.case('Weaver of Lightning','cleanup_0')['steps'];cut=next(i for i,x in enumerate(s) if x['op']=='cleanup')
        self.assertEqual(self.final_value(s[:cut],'assert_card',ref='enemy',property='damage'),1)
        self.assertEqual(self.final_value(s,'assert_card',ref='enemy',property='damage'),0)

    def test_mana_scaled_zero_damage_still_requires_a_target_offer_and_unequal_values_remain_exact(self):
        for owner in (0,1):
            for mode,life in [('zero_value',20),('alternate_spell',19),('effect',18),('high_value',12)]:
                s=self.case('Kaervek the Merciless',f'{mode}_{owner}')['steps']
                self.assertEqual(self.final_value(s,'assert_life',player=1-owner),life)
                self.assertIn(dict(op='assert_unused_choices',value=0),s)
                self.assertIn(dict(op='choose_player',player=owner,value=1-owner),s)

    def test_ordinal_damage_uses_correct_second_cast_intent_and_bounded_unexpected_target_probes(self):
        for owner in (0,1):
            s=self.case("Pyromancer's Assault",f'third_spell_{owner}')['steps']
            self.assertEqual(sum(x['op']=='cast' for x in s),3)
            self.assertEqual(sum(x['op']=='announce_spell' for x in s),3)
            self.assertEqual([x['value'] for x in s if x['op']=='assert_unused_choices'],[1,0,1])
            self.assertEqual(sum(x['op']=='choose_player' for x in s),1)
            self.assertEqual(sum(x['op']=='choose_optional_target' for x in s),2)
            self.assertEqual(self.final_value(s,'assert_life',player=1-owner),18)
            s=self.case("Pyromancer's Assault",f'prior_turn_reset_{owner}')['steps']
            self.assertEqual(sum(x['op']=='advance_turn' for x in s),2)
            self.assertEqual(self.final_value(s,'assert_life',player=1-owner),16)

    def test_denied_damage_casts_do_not_queue_unreachable_target_or_trigger_inputs(self):
        for name in ('Weaver of Lightning','Lys Alana Bowmaster','Pyroconvergence',"Pyromancer's Assault"):
            for owner in (0,1):
                s=self.case(name,f'trigger_insufficient_mana_{owner}')['steps']
                self.assertIn(dict(op='cast',ref='spell_0',value=0),s)
                self.assertFalse(any(x['op'] in ('choose_target','choose_player','choose_optional_target','choose_trigger_source') for x in s))

    def test_modal_overlap_witness_is_a_real_paid_cast_and_new_characteristics_are_pinned(self):
        for owner in (0,1):
            c=self.case('Mindsparker',f'overlap_once_{owner}');s=c['steps'];actor=1-owner
            cast=next(i for i,x in enumerate(s) if x['op']=='cast')
            self.assertEqual(s[cast-1],dict(op='choose_dialog',player=actor,value=1))
            self.assertIn(dict(op='present_ai_dialogs',value=1),s[:cast])
            self.assertEqual(self.final_value(s,'assert_life',player=actor),18)
            self.assertEqual(self.library_delta('Mindsparker',f'overlap_once_{owner}',actor),1)
        for name,key,value in [('Chandra, Pyrogenius','loyalty','4'),('Elvish Mystic','type_line','Creature — Human'),('Azorius Charm','oracle_text','Draw two cards.')]:
            o=copy.deepcopy(self.oracle);o[name][key]=value
            with self.assertRaises(ValueError):f.validate_witnesses(o,self.rows)
        r=copy.deepcopy(self.rows);r['Azorius Charm']['handler']='card_other'
        with self.assertRaises(ValueError):f.validate_witnesses(self.oracle,r)

    def test_bowmaster_presentation_is_source_pinned_and_does_not_make_self_cast_a_trigger(self):
        d=self.declaration('Lys Alana Bowmaster');self.assertTrue(d['params']['optional'])
        rows=copy.deepcopy(self.rows);rows['Lys Alana Bowmaster']['handler']='card_other'
        with self.assertRaises(ValueError):f.validate_witnesses(self.oracle,rows)
        for owner in (0,1):
            s=self.case('Lys Alana Bowmaster',f'source_cast_{owner}')['steps']
            first=next(i for i,x in enumerate(s) if x['op']=='cast' and x['ref']=='source')
            second=next(i for i,x in enumerate(s) if x['op']=='cast' and x['ref']=='spell_0')
            self.assertFalse(any(x['op'] in ('choose_target','choose_optional_target','choose_trigger_source') for x in s[first:second]))
            self.assertEqual(self.final_value(s,'assert_card',ref='flyer_enemy',property='damage'),2)

    def test_four_followons_are_exact_calibrated_sources_with_full_cost_and_variant_pins(self):
        expected={'Eidolon of the Great Revel':(46,[0,0,0,0,2,0]),'Aether Sting':(42,[3,0,0,0,1,0]),
                  'Spellshock':(40,[2,0,0,0,1,0]),'Electrostatic Field':(42,[1,0,0,0,1,0])}
        self.assertEqual(sum(count for count,mana in expected.values()),170)
        for name,(count,mana) in expected.items():
            p=f.classify(self.oracle[name]);self.assertEqual(p['mechanism'],'cast_damage');self.assertEqual(p['mana'],mana)
            self.assertEqual(len(f.variants(p)),count)
            altered=copy.deepcopy(self.oracle[name]);altered['oracle_text']+='\n{T}: Add {R}.'
            with self.assertRaises(ValueError):f.classify(altered)
        field=f.classify(self.oracle['Electrostatic Field'])
        self.assertEqual((field['keywords'],field['power'],field['toughness']),(['Defender'],0,4))

    def test_followon_both_caster_and_opponent_creature_scopes_have_correct_damage_receipts(self):
        for owner in (0,1):
            for name in ('Eidolon of the Great Revel','Spellshock'):
                for mode,caster in [('effect',owner),('opponent_cast',1-owner)]:
                    s=self.case(name,f'{mode}_{owner}')['steps']
                    self.assertEqual(self.final_value(s,'assert_life',player=caster),18)
                    self.assertEqual(self.final_value(s,'assert_life',player=1-caster),20)
                s=self.case(name,f'countered_trigger_{owner}')['steps']
                self.assertEqual(self.final_value(s,'assert_life',player=owner),18)
                self.assertEqual(self.final_value(s,'assert_life',player=1-owner),18)
            for mode,enemy_life in [('effect',19),('countered_trigger',19),('opponent_cast',20),('nonmatching_spell',20)]:
                s=self.case('Aether Sting',f'{mode}_{owner}')['steps']
                self.assertEqual(self.final_value(s,'assert_life',player=owner),20)
                self.assertEqual(self.final_value(s,'assert_life',player=1-owner),enemy_life)
            for mode,life in [('effect',18),('zero_value',18),('above_threshold',20)]:
                self.assertEqual(self.final_value(self.case('Eidolon of the Great Revel',f'{mode}_{owner}')['steps'],'assert_life',player=owner),life)

    def test_followon_instant_and_sorcery_effects_keep_their_separate_payment_and_draw_receipts(self):
        for owner in (0,1):
            for mode,opponent_life,draws in [('effect',19,0),('alternate_spell',19,2),('nonmatching_spell',20,0),('opponent_cast',20,0)]:
                s=self.case('Electrostatic Field',f'{mode}_{owner}')['steps']
                self.assertEqual(self.final_value(s,'assert_life',player=owner),20)
                self.assertEqual(self.final_value(s,'assert_life',player=1-owner),opponent_life)
                self.assertEqual(self.library_delta('Electrostatic Field',f'{mode}_{owner}',owner),draws)
                self.assertEqual(self.final_value(s,'assert_card',ref='source',property='toughness'),4)
            self.assertEqual(self.final_value(self.case('Electrostatic Field',f'trigger_payment_{owner}')['steps'],'assert_mana_total',player=owner),0)


if __name__=='__main__':unittest.main()
