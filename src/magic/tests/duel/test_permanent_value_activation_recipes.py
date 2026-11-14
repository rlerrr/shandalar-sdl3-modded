"""Independent Oracle receipts and fail-closed activated-value contracts."""
import copy
import json
from pathlib import Path
import unittest
from generate_cases import registry
from permanent_value_activation_recipes import FAMILY,parse,spec,variants,expand,validate_witnesses,parse_cost,pins,validate_unique_value_mode,derived_amount

class PermanentValueActivationRecipesTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows={r['name']:r for r in registry()}
    def declaration(self,name,variant='value_0',ability=0):
        ui=pins().get('source_interfaces',{}).get(name,{}).get('abilities',{}).get(str(ability),{})
        return dict(family=FAMILY,version=2,oracle_text=self.oracle[name]['oracle_text'],params=dict(ability=ability,**ui.get('params',{})),variants=[variant],**{k:v for k,v in ui.items() if k!='params'})
    def case(self,name,variant='value_0',ability=0):
        d=self.declaration(name,variant,ability);p=spec(d,self.oracle[name])
        return expand(name,self.rows[name]['csvid'],d,p,[variant],self.rows,self.oracle)[0]
    def last(self,case,op,**fields):
        return [s for s in case['steps'] if s['op']==op and all(s.get(k)==v for k,v in fields.items())][-1]['value']
    def test_eight_printed_positive_effects_have_independent_life_hand_library_receipts(self):
        expected={'Carnage Altar':(20,2,11),'Sage of Lat-Nam':(20,2,11),'Overgrown Estate':(23,1,12),
                  'Vampiric Rites':(21,2,11),'Moriok Replica':(18,3,10),'Drowned Rusalka':(20,1,11),
                  'Scepter of Fugue':(20,1,12),'Sands of Delirium':(20,1,12)}
        for owner in (0,1):
            for name,(life,hand,library) in expected.items():
                with self.subTest(name=name,owner=owner):
                    c=self.case(name,f'value_{owner}')
                    self.assertEqual(self.last(c,'assert_life',player=owner),life)
                    self.assertEqual(self.last(c,'assert_zone',player=owner,zone='hand'),hand)
                    self.assertEqual(self.last(c,'assert_zone',player=owner,zone='library'),library)
    def test_sacrifice_costs_are_paid_before_draw_with_opponent_eligible_control_untouched(self):
        c=self.case('Carnage Altar','pending_snapshot_0');s=c['steps'];cut=next(i for i,a in enumerate(s) if a['op']=='capture')
        self.assertIn(dict(op='require_card',ref='victim',property='in_play',value=0),s[:cut])
        self.assertIn(dict(op='require_card',ref='enemy_eligible',property='in_play',value=1),s[:cut])
        self.assertIn(dict(op='assert_zone',player=0,zone='hand',value=1),s[:cut])
        self.assertIn(dict(op='assert_zone',player=0,zone='library',value=12),s[:cut])
        self.assertEqual(self.last(c,'assert_zone',player=0,zone='hand'),2)
    def test_self_sacrifice_and_draw_then_life_loss(self):
        c=self.case('Moriok Replica','value_1')
        self.assertEqual(self.last(c,'require_card',ref='source',property='in_play'),0)
        self.assertEqual(self.last(c,'assert_life',player=1),18)
        self.assertEqual(self.last(c,'assert_zone_multiplicity',player=1,zone='graveyard',index=self.rows['Moriok Replica']['csvid']),1)
        self.assertEqual(self.last(c,'assert_zone_multiplicity',player=1,zone='hand',index=self.rows['Plains']['csvid']),2)
    def test_discard_then_draw_has_distinct_known_destinations(self):
        c=self.case('Drowned Rusalka','value_1')
        self.assertEqual(self.last(c,'assert_zone_multiplicity',player=1,zone='graveyard',index=self.rows['Plains']['csvid']),1)
        self.assertEqual(self.last(c,'assert_zone_multiplicity',player=1,zone='hand',index=self.rows['Forest']['csvid']),1)
    def test_discard_from_empty_hand_does_not_prevent_the_later_draw(self):
        c=self.case('Drowned Rusalka','empty_hand_0')
        self.assertIn(dict(op='activate',ref='source',value=1),c['steps'])
        self.assertEqual(self.last(c,'assert_zone',player=0,zone='hand'),1)
    def test_targeted_discard_checks_both_self_and_opponent(self):
        for owner in (0,1):
            for mode,target in [('value',1-owner),('target_self',owner)]:
                c=self.case('Scepter of Fugue',f'{mode}_{owner}')
                self.assertIn(dict(op='choose_player',player=owner,value=target),c['steps'])
                self.assertEqual(self.last(c,'assert_zone',player=target,zone='hand'),0)
                self.assertEqual(self.last(c,'assert_zone',player=1-target,zone='hand'),1)
    def test_x_payments_and_short_or_empty_mill_do_not_fabricate_cards(self):
        for owner in (0,1):
            for mode,x,left,grave in [('x0',0,12,1),('x1',1,11,2),('x5',5,7,6),('short_library',3,0,3),('empty_library',3,0,1)]:
                c=self.case('Sands of Delirium',f'{mode}_{owner}')
                self.assertIn(dict(op='choose_x',player=owner,value=x),c['steps'])
                self.assertEqual(self.last(c,'assert_zone',player=1-owner,zone='library'),left)
                self.assertEqual(self.last(c,'assert_zone',player=1-owner,zone='graveyard'),grave)
    def test_exact_colored_and_generic_ability_payments_leave_zero(self):
        for name in ('Carnage Altar','Vampiric Rites','Sands of Delirium'):
            c=self.case(name,'payment_0')
            self.assertEqual(self.last(c,'assert_mana_total',player=0),0)
            self.assertEqual(sum(a['value'] for a in c['steps'] if a['op']=='set_mana' and a['player']==0),3 if name!='Vampiric Rites' else 2)
    def test_source_cast_cost_is_asserted_before_activation_budget_reset(self):
        c=self.case('Moriok Replica','cast_0');s=c['steps']
        self.assertIn(dict(op='assert_mana_total',player=0,value=77),s)
        self.assertLess(next(i for i,a in enumerate(s) if a.get('ref')=='source' and a['op']=='cast'),next(i for i,a in enumerate(s) if a.get('ref')=='source' and a['op']=='activate'))
    def test_tap_and_summoning_sickness_are_separate_from_nontap_permissions(self):
        for mode in ('tapped','sick','fresh','cast'):
            self.assertIn(dict(op='require_can_activate',ref='source',value=0),self.case('Sage of Lat-Nam',mode+'_0')['steps'])
            self.assertIn(dict(op='require_can_activate',ref='source',value=1),self.case('Moriok Replica',mode+'_0')['steps'])
    def test_newly_cast_tap_source_preserves_resources_until_sickness_expires(self):
        for owner in (0,1):
            c=self.case('Sage of Lat-Nam',f'cast_{owner}')
            self.assertIn(dict(op='require_can_activate',ref='source',value=0),c['steps'])
            self.assertFalse(any(s['op']=='activate' for s in c['steps']))
            self.assertIn(dict(op='activate',ref='source',value=0),self.case('Sage of Lat-Nam',f'fresh_{owner}')['steps'])
            self.assertEqual(self.last(c,'require_card',ref='victim',property='in_play'),1)
            self.assertEqual(self.last(c,'require_card',ref='source',property='tapped'),0)
            self.assertEqual(self.last(c,'assert_zone',player=owner,zone='hand'),1)
            self.assertEqual(self.last(c,'assert_zone',player=owner,zone='library'),12)
            self.assertEqual(self.last(c,'assert_zone',player=owner,zone='graveyard'),1)
    def test_printed_turn_restriction_has_a_real_negative_control(self):
        self.assertIn(dict(op='require_can_activate',ref='source',value=0),self.case('Scepter of Fugue','offturn_1')['steps'])
        self.assertIn(dict(op='require_can_activate',ref='source',value=1),self.case('Sands of Delirium','offturn_1')['steps'])
    def test_pending_ability_survives_source_loss(self):
        c=self.case('Sands of Delirium','source_destroyed_pending_1');s=c['steps']
        self.assertLess(next(i for i,a in enumerate(s) if a['op']=='activate'),next(i for i,a in enumerate(s) if a['op']=='destroy'))
        self.assertLess(next(i for i,a in enumerate(s) if a['op']=='destroy'),next(i for i,a in enumerate(s) if a['op']=='resolve'))
        self.assertEqual(self.last(c,'assert_zone',player=0,zone='library'),9)
    def test_snapshots_replay_real_costs_and_do_not_share_mutable_steps(self):
        for mode in ('ready_snapshot','pending_snapshot','resolved_snapshot'):
            c=self.case('Vampiric Rites',mode+'_1');ops=[s['op'] for s in c['steps']]
            self.assertIn('capture',ops);self.assertIn('ai_roundtrip',ops);self.assertIn('restore',ops)
            self.assertEqual(ops[-1],'assert_recorded_state')
            self.assertEqual(len({id(s) for s in c['steps']}),len(c['steps']))
    def test_whole_text_family_version_and_parameters_fail_closed(self):
        d=self.declaration('Carnage Altar');r=self.oracle['Carnage Altar']
        for change in (dict(family='other'),dict(version=True),dict(version=1),dict(version=2.0),dict(params={}),dict(params=dict(ability=True)),dict(oracle_text='changed')):
            with self.subTest(change=change),self.assertRaises(ValueError):spec(dict(d,**change),r)
    def test_unknown_duplicate_empty_and_nonstring_variants_are_rejected(self):
        d=self.declaration('Carnage Altar')
        for selected in ([],None,['value_0','value_0'],['unknown_0'],['value_2'],[1]):
            with self.subTest(selected=selected),self.assertRaises(ValueError):spec(dict(d,variants=selected),self.oracle['Carnage Altar'])
    def test_unreviewed_costs_effects_multiface_or_nonnumeric_source_are_rejected(self):
        r=self.oracle['Carnage Altar']
        for change in (dict(oracle_text='{2}{X}{X}: Draw a card.'),dict(oracle_text='{3}: Draw a card and create a Treasure token.'),dict(oracle_text='Remove a counter: Draw a card.'),dict(layout='transform'),dict(card_faces=[{}])):
            with self.subTest(change=change),self.assertRaises(ValueError):parse(dict(r,**change),0)
        with self.assertRaises(ValueError):parse(dict(self.oracle['Sage of Lat-Nam'],power='*'),0)
    def test_witness_changes_in_costs_stats_types_or_identity_are_rejected(self):
        for name,field,value in [('Ornithopter','toughness','3'),('Howling Mine','type_line','Creature'),('Plains','oracle_id','wrong')]:
            o=copy.deepcopy(self.oracle);o[name][field]=value
            with self.subTest(name=name),self.assertRaises(ValueError):validate_witnesses(o,self.rows)
    def test_named_counter_cost_cannot_skip_actual_counter_acquisition(self):
        self.assertEqual(parse_cost('{T}, Remove a charge counter from this artifact')['counter'],dict(kind='charge',amount=1))
        with self.assertRaises(ValueError):parse(dict(self.oracle['Carnage Altar'],oracle_text='{T}, Remove a charge counter from this artifact: Draw a card.'),0)
    def test_repeated_cost_components_cannot_overwrite_the_real_printed_payment(self):
        for cost in ('{1}, {2}','Pay 1 life, Pay 2 life','Discard a card, Discard two cards','{T}, {T}'):
            with self.subTest(cost=cost),self.assertRaises(ValueError):parse_cost(cost)
    def test_whole_hand_and_exact_card_discard_costs_have_different_empty_hand_permissions(self):
        r=dict(self.oracle['Carnage Altar'],oracle_text='Discard your hand: Draw a card.')
        d=dict(family=FAMILY,version=2,oracle_text=r['oracle_text'],params=dict(ability=0),variants=['empty_hand_0'])
        p=spec(d,r);o=dict(self.oracle,**{'Carnage Altar':r});c=expand('Carnage Altar',self.rows['Carnage Altar']['csvid'],d,p,d['variants'],self.rows,o)[0]
        self.assertIn(dict(op='require_can_activate',ref='source',value=1),c['steps'])
        r=dict(r,oracle_text='Discard two cards: Draw a card.');d=dict(d,oracle_text=r['oracle_text'],variants=['value_0']);p=spec(d,r);o=dict(self.oracle,**{'Carnage Altar':r})
        c=expand('Carnage Altar',self.rows['Carnage Altar']['csvid'],d,p,d['variants'],self.rows,o)[0]
        self.assertIn(dict(op='require_can_activate',ref='source',value=0),c['steps'])
    def test_legacy_sacrifice_menu_can_explicitly_request_the_ai_controller_input(self):
        d=dict(self.declaration('Overgrown Estate','value_1'),params=dict(ability=0,sacrifice_choices={'1':True}));p=spec(d,self.oracle['Overgrown Estate'])
        c=expand('Overgrown Estate',self.rows['Overgrown Estate']['csvid'],d,p,d['variants'],self.rows,self.oracle)[0]
        self.assertIn(dict(op='choose_target',player=1,ref='victim'),c['steps'])
        self.assertEqual(self.last(c,'assert_life',player=1),23)
        self.assertEqual(self.last(c,'assert_zone_multiplicity',player=1,zone='graveyard',index=self.rows['Forest']['csvid']),1)
        self.assertNotIn(dict(op='choose_target',player=1,ref='victim'),self.case('Carnage Altar','value_1')['steps'])
    def test_sacrifice_presentation_is_strict_and_cannot_add_a_choice_to_a_self_cost(self):
        for choices in ({'2':True},{'1':1},[],{'1':'yes'}):
            with self.subTest(choices=choices),self.assertRaises(ValueError):spec(dict(self.declaration('Overgrown Estate'),params=dict(ability=0,sacrifice_choices=choices)),self.oracle['Overgrown Estate'])
        with self.assertRaises(ValueError):spec(dict(self.declaration('Moriok Replica'),params=dict(ability=0,sacrifice_choices={'1':True})),self.oracle['Moriok Replica'])
    def test_actual_source_cast_acquires_and_pays_charge_counters_before_milling(self):
        for owner in (0,1):
            c=self.case('Shriekhorn',f'value_{owner}',1);s=c['steps']
            self.assertEqual(next(card['zone'] for card in c['setup'] if card['ref']=='source'),'hand')
            self.assertIn(dict(op='assert_counter',ref='source',counter_kind='charge',value=3),s)
            self.assertIn(dict(op='assert_counter',ref='source',counter_kind='charge',value=2),s)
            self.assertEqual(self.last(c,'assert_zone',player=1-owner,zone='library'),10)
            self.assertEqual(self.last(c,'assert_zone',player=1-owner,zone='graveyard'),3)
    def test_serum_tank_uses_actual_entry_trigger_and_draws_only_after_spending_counter(self):
        c=self.case('Serum Tank','pending_snapshot_1',1);s=c['steps'];capture=next(i for i,a in enumerate(s) if a['op']=='capture')
        self.assertIn(dict(op='assert_counter',ref='source',counter_kind='charge',value=1),s[:capture])
        self.assertIn(dict(op='assert_counter',ref='source',counter_kind='charge',value=0),s[:capture])
        self.assertIn(dict(op='assert_zone',player=1,zone='hand',value=1),s[:capture])
        self.assertEqual(self.last(c,'assert_zone',player=1,zone='hand'),2)
        self.assertTrue(any(a['op']=='assert_counter' for a in s[next(i for i,a in enumerate(s) if a['op']=='restore')+1:]))
    def test_minus_counters_change_real_stats_and_are_paid_before_target_draw(self):
        c=self.case('Etched Monstrosity','value_0',1);s=c['steps']
        self.assertIn(dict(op='require_card',ref='source',property='power',value=5),s)
        self.assertIn(dict(op='require_card',ref='source',property='power',value=10),s)
        self.assertIn(dict(op='assert_counter',ref='source',counter_kind='minus1',value=5),s)
        self.assertIn(dict(op='assert_counter',ref='source',counter_kind='minus1',value=0),s)
        self.assertEqual(self.last(c,'assert_zone',player=1,zone='hand'),4)
    def test_loyalty_is_paid_at_activation_and_life_draw_effects_are_independent(self):
        for name,ability,initial,after,life,hand in [('Ajani Goldmane',0,4,5,22,1),('Jace Beleren',1,3,2,20,1),('Teferi, Timebender',1,5,2,22,3)]:
            for owner in (0,1):
                with self.subTest(name=name,owner=owner):
                    c=self.case(name,f'value_{owner}',ability);s=c['steps']
                    self.assertIn(dict(op='assert_counter',ref='source',counter_kind='loyalty',value=initial),s)
                    self.assertIn(dict(op='assert_counter',ref='source',counter_kind='loyalty',value=after),s)
                    self.assertEqual(self.last(c,'assert_life',player=owner),life)
                    self.assertEqual(self.last(c,'assert_zone',player=owner,zone='hand'),hand)
    def test_jace_symmetric_draw_preserves_both_owners_and_uses_plus_two_cost(self):
        c=self.case('Jace Beleren','value_0',0)
        self.assertEqual(self.last(c,'assert_zone',player=0,zone='hand'),2)
        self.assertEqual(self.last(c,'assert_zone',player=1,zone='hand'),2)
        self.assertIn(dict(op='assert_counter',ref='source',counter_kind='loyalty',value=5),c['steps'])
    def test_planeswalker_timing_and_once_per_turn_are_checked_through_real_queries(self):
        for owner in (0,1):
            self.assertIn(dict(op='require_can_activate',ref='source',value=0),self.case('Ajani Goldmane',f'offturn_{owner}')['steps'])
            c=self.case('Jace Beleren',f'repeat_denied_{owner}',1)
            self.assertEqual([a['value'] for a in c['steps'] if a['op']=='require_can_activate' and a['ref']=='source'],[1,0])
    def test_tapping_counter_source_uses_a_real_opposing_icy_activation(self):
        c=self.case('Shriekhorn','tapped_1',1);s=c['steps']
        self.assertIn(dict(op='activate',ref='counter_tapper',value=1),s)
        self.assertIn(dict(op='choose_target',player=0,ref='source'),s)
        self.assertIn(dict(op='require_can_activate',ref='source',value=0),s)
        self.assertIn(dict(op='assert_counter',ref='source',counter_kind='charge',value=3),s)
    def test_spent_counter_permission_is_checked_after_real_untapping(self):
        c=self.case('Serum Tank','spent_counter_0',1);s=c['steps']
        self.assertIn(dict(op='cast',ref='counter_untapper',value=1),s)
        self.assertIn(dict(op='choose_dialog',player=0,value=1),s)
        self.assertEqual(self.last(c,'require_card',ref='source',property='tapped'),0)
        self.assertEqual(self.last(c,'assert_mana_total',player=0),76)
        self.assertEqual(self.last(c,'assert_zone',player=0,zone='graveyard'),2)
        self.assertIn(dict(op='require_can_activate',ref='source',value=0),s)
    def test_counter_models_never_set_counters_and_snapshot_receipts_are_not_aliased(self):
        c=self.case('Etched Monstrosity','pending_snapshot_1',1);s=c['steps']
        self.assertFalse(any(a['op'].startswith('set_counter') for a in s))
        self.assertEqual(len({id(a) for a in s}),len(s))
    def test_seven_fixed_counter_profiles_pay_real_counters_and_preserve_distinct_effects(self):
        profiles=[('Jace, Memory Adept',1,'loyalty',4,4,1),('Liliana of the Veil',0,'loyalty',3,4,0),
                  ('Liliana Vess',0,'loyalty',5,6,0),('Trigon of Mending',2,'charge',3,2,0),
                  ('Trigon of Thought',2,'charge',3,2,0),('Dovin Baan',1,'loyalty',3,2,1),
                  ("Nissa, Nature's Artisan",0,'loyalty',5,8,0)]
        for name,ability,kind,initial,after,choice in profiles:
            for owner in (0,1):
                with self.subTest(name=name,owner=owner):
                    c=self.case(name,f'value_{owner}',ability);s=c['steps'];other=1-owner
                    self.assertIn(dict(op='cast',ref='source',value=1),s)
                    self.assertIn(dict(op='assert_counter',ref='source',counter_kind=kind,value=initial),s)
                    self.assertIn(dict(op='choose_dialog',player=owner,value=choice),s)
                    activation=next(i for i,a in enumerate(s) if a['op']=='activate' and a['ref']=='source')
                    self.assertEqual(s[activation+1],dict(op='assert_counter',ref='source',counter_kind=kind,value=after))
                    self.assertFalse(any(a['op'].startswith('set_counter') for a in s))
                    if name=='Jace, Memory Adept':
                        self.assertEqual(self.last(c,'assert_zone',player=other,zone='library'),2)
                        self.assertEqual(self.last(c,'assert_zone',player=other,zone='graveyard'),11)
                    elif name in ('Liliana Vess','Liliana of the Veil'):
                        self.assertEqual(self.last(c,'assert_zone',player=other,zone='hand'),0)
                        self.assertEqual(self.last(c,'assert_zone',player=owner,zone='hand'),0 if name=='Liliana of the Veil' else 1)
                    elif name=='Trigon of Mending':self.assertEqual(self.last(c,'assert_life',player=other),23)
                    elif name=='Trigon of Thought':self.assertEqual(self.last(c,'assert_zone',player=owner,zone='hand'),2)
                    elif name=='Dovin Baan':
                        self.assertEqual(self.last(c,'assert_life',player=owner),22)
                        self.assertEqual(self.last(c,'assert_zone',player=owner,zone='hand'),2)
                    else:self.assertEqual(self.last(c,'assert_life',player=owner),23)
    def test_new_fixed_counter_profiles_keep_zero_loyalty_cost_and_partial_mill_semantics(self):
        for owner in (0,1):
            for mode,grave in (('short_library',3),('empty_library',1)):
                c=self.case('Jace, Memory Adept',f'{mode}_{owner}',1)
                self.assertEqual(self.last(c,'assert_zone',player=1-owner,zone='library'),0)
                self.assertEqual(self.last(c,'assert_zone',player=1-owner,zone='graveyard'),grave)
                self.assertEqual(self.last(c,'assert_counter',ref='source',counter_kind='loyalty'),4)
            c=self.case('Trigon of Mending',f'target_self_{owner}',2)
            self.assertEqual(self.last(c,'assert_life',player=owner),23)
            self.assertEqual(self.last(c,'assert_life',player=1-owner),20)
    def test_derived_cost_x_is_fixed_by_oracle_hand_size_instead_of_a_chosen_x(self):
        p=parse(self.oracle['Bargaining Table'],0)
        self.assertFalse(p['cost']['x'])
        self.assertEqual(p['derived_cost'],dict(kind='zone_count',scope='opponent',zone='hand'))
        for owner in (0,1):
            for mode,total,hand in [('cost_zero',80,2),('cost_many',77,2),('payment',0,2),('insufficient_mana',2,1)]:
                c=self.case('Bargaining Table',f'{mode}_{owner}')
                self.assertFalse(any(s['op']=='choose_x' for s in c['steps']))
                self.assertEqual(self.last(c,'assert_mana_total',player=owner),total)
                self.assertEqual(self.last(c,'assert_zone',player=owner,zone='hand'),hand)
    def test_derived_sacrifice_uses_actual_pumped_stats_before_cost_and_retains_lki_after_source_removal(self):
        for owner in (0,1):
            diamond=self.case('Diamond Valley',f'stat_pumped_{owner}')
            altar=self.case('Altar of Dementia',f'stat_pumped_{owner}')
            for c in (diamond,altar):
                s=c['steps'];activation=next(i for i,a in enumerate(s) if a['op']=='activate')
                self.assertIn(dict(op='cast',ref='stat_spell',value=1),s[:activation])
                self.assertIn(dict(op='require_card',ref='victim',property='power',value=3),s[:activation])
                self.assertIn(dict(op='require_card',ref='victim',property='toughness',value=5),s[:activation])
                self.assertEqual(self.last(c,'require_card',ref='victim',property='in_play'),0)
                self.assertEqual(self.last(c,'assert_mana_total',player=owner),79)
            self.assertEqual(self.last(diamond,'assert_life',player=owner),25)
            self.assertEqual(self.last(altar,'assert_zone',player=1-owner,zone='library'),9)
            removed=self.case('Diamond Valley',f'source_destroyed_pending_{owner}')
            self.assertEqual(self.last(removed,'assert_life',player=owner),22)
            self.assertEqual(self.last(removed,'require_card',ref='source',property='in_play'),0)
            zero=self.case('Altar of Dementia',f'zero_stat_{owner}')
            self.assertEqual(self.last(zero,'assert_zone',player=1-owner,zone='library'),12)
            self.assertEqual(self.last(zero,'require_card',ref='victim',property='in_play'),0)
    def test_derived_live_counts_distinguish_ownership_subtype_color_and_removed_members(self):
        expectations=[('Elspeth Tirel',0,'value',22),('Elspeth Tirel',0,'metric_removed',21),
                      ('Wellwisher',0,'value',23),('Wellwisher',0,'metric_removed',22),('Wellwisher',0,'no_other_members',21),
                      ('Wellwisher',0,'zero_after_source_removal',20),('Tomb of the Spirit Dragon',1,'value',21),
                      ('Tomb of the Spirit Dragon',1,'metric_many',23),('Tomb of the Spirit Dragon',1,'no_other_members',20)]
        for name,ability,mode,life in expectations:
            for owner in (0,1):
                c=self.case(name,f'{mode}_{owner}',ability)
                self.assertEqual(self.last(c,'assert_life',player=owner),life,(name,mode,owner))
                if mode=='metric_removed':
                    s=c['steps'];self.assertLess(next(i for i,a in enumerate(s) if a['op']=='activate'),next(i for i,a in enumerate(s) if a['op']=='destroy'))
    def test_derived_grave_count_is_measured_on_resolution_once_and_partial_mill_is_bounded(self):
        for owner in (0,1):
            for mode,library,grave in [('value',11,2),('metric_many',9,6),('metric_removed',10,4),('short_library',0,5),('empty_library',0,1),('bare_zones',12,0)]:
                c=self.case('Keening Stone',f'{mode}_{owner}')
                self.assertEqual(self.last(c,'assert_zone',player=1-owner,zone='library'),library,(mode,owner))
                self.assertEqual(self.last(c,'assert_zone',player=1-owner,zone='graveyard'),grave,(mode,owner))
    def test_derived_maximum_power_uses_live_own_creatures_and_real_pump_expires(self):
        for owner in (0,1):
            for mode,life in [('value',23),('metric_removed',22),('no_other_members',20),('stat_pumped',26),('stat_pumped_cleanup',26)]:
                c=self.case('Huatli, Warrior Poet',f'{mode}_{owner}',0)
                self.assertEqual(self.last(c,'assert_life',player=owner),life)
                self.assertEqual(self.last(c,'assert_counter',ref='source',counter_kind='loyalty'),5)
                if mode=='stat_pumped_cleanup':self.assertEqual(self.last(c,'require_card',ref='member',property='power'),3)
            elspeth=self.case('Elspeth Tirel',f'value_{owner}')
            self.assertEqual(self.last(elspeth,'assert_counter',ref='source',counter_kind='loyalty'),6)
    def test_derived_expression_bounds_and_complete_oracle_clauses_fail_closed(self):
        self.assertIsNone(parse(self.oracle['Wellwisher'],0)['effects'][0]['amount']['type'])
        for expr in (True,-1,65,dict(kind='custom')):
            with self.subTest(expr=expr),self.assertRaises(ValueError):derived_amount(expr,0,1,{},dict(hand=[[],[]]),{})
        for suffix in (' Then draw a card.',' unless you control an Elf.'):
            r=dict(self.oracle['Wellwisher'],oracle_text=self.oracle['Wellwisher']['oracle_text']+suffix)
            with self.assertRaises(ValueError):parse(r,0)
        with self.assertRaises(ValueError):self.case('Diamond Valley','zero_stat_0')
    def test_derived_mana_sibling_mode_receipt_is_immediate_and_uses_the_exact_printed_budget(self):
        for owner in (0,1):
            c=self.case('Tomb of the Spirit Dragon',f'payment_{owner}',1);s=c['steps']
            activation=next(i for i,a in enumerate(s) if a['op']=='activate')
            self.assertEqual(s[activation+1],dict(op='require_activation_mode',value=1))
            self.assertIn(dict(op='choose_activation_mode',ref='source',player=owner,mode_label='Gain life',fallback_color=0,allow_auto_nonmana=True),s[:activation])
            self.assertEqual(self.last(c,'assert_mana_total',player=owner),0)
            self.assertEqual(self.last(c,'assert_life',player=owner),21)
        self.assertNotIn('insufficient_mana_0',variants(parse(self.oracle['Tomb of the Spirit Dragon'],1)))
    def test_six_derived_expansion_profiles_keep_real_costs_and_independent_effect_receipts(self):
        profiles=[('Disciple of Griselbrand',0,21,1,79,0),('Miren, the Moaning Well',1,22,1,77,1),
                  ('Nissa Revane',1,22,1,80,1),('Riptide Director',0,20,3,76,1),
                  ('Sea Gate Loremaster',0,20,3,80,1),('Blighted Steppe',1,24,1,76,0)]
        for name,ability,life,hand,mana_total,alive in profiles:
            for owner in (0,1):
                with self.subTest(name=name,owner=owner):
                    c=self.case(name,f'value_{owner}',ability)
                    self.assertEqual(self.last(c,'assert_life',player=owner),life)
                    self.assertEqual(self.last(c,'assert_zone',player=owner,zone='hand'),hand)
                    self.assertEqual(self.last(c,'assert_mana_total',player=owner),mana_total)
                    self.assertEqual(self.last(c,'require_card',ref='source',property='in_play'),alive)
    def test_source_creature_can_pay_its_own_sacrifice_even_when_fresh_and_keeps_pumped_lki(self):
        for owner in (0,1):
            for mode,life in [('value',21),('sick',21),('fresh',21),('stat_pumped',24)]:
                c=self.case('Disciple of Griselbrand',f'{mode}_{owner}')
                self.assertFalse(any(card['ref']=='victim' for card in c['setup']))
                self.assertIn(dict(op='require_can_activate',ref='source',value=1),c['steps'])
                self.assertEqual(self.last(c,'assert_life',player=owner),life)
                self.assertEqual(self.last(c,'require_card',ref='source',property='in_play'),0)
                self.assertEqual(self.last(c,'assert_zone_multiplicity',player=owner,zone='graveyard',index=self.rows['Disciple of Griselbrand']['csvid']),1)
        self.assertNotIn('no_victim_0',variants(parse(self.oracle['Disciple of Griselbrand'],0)))
    def test_own_subtype_counts_use_actual_qualifying_witnesses_and_recompute_after_removal(self):
        for owner in (0,1):
            nissa=self.case('Nissa Revane',f'metric_many_{owner}',1)
            self.assertEqual(next(card['name'] for card in nissa['setup'] if card['ref']=='source'),'Nissa Revane')
            self.assertEqual(next(card['name'] for card in nissa['setup'] if card['ref']=='member'),'Wellwisher')
            self.assertEqual(self.last(nissa,'assert_life',player=owner),26)
            self.assertEqual(self.last(nissa,'assert_counter',ref='source',counter_kind='loyalty'),3)
            removed=self.case('Nissa Revane',f'metric_removed_{owner}',1)
            self.assertEqual(self.last(removed,'assert_life',player=owner),20)
            for name in ('Riptide Director','Sea Gate Loremaster'):
                for mode,hand in [('metric_removed',2),('source_destroyed_pending',2),('no_other_members',2),('zero_after_source_removal',1)]:
                    c=self.case(name,f'{mode}_{owner}')
                    self.assertEqual(self.last(c,'assert_zone',player=owner,zone='hand'),hand,(name,mode))
                    self.assertEqual(self.last(c,'assert_zone',player=owner,zone='library'),13-hand)
    def test_count_effect_self_sacrifice_moves_the_actual_source_before_resolution(self):
        for owner in (0,1):
            for mode,life in [('value',24),('metric_removed',22),('no_other_members',20)]:
                c=self.case('Blighted Steppe',f'{mode}_{owner}',1);s=c['steps']
                activation=next(i for i,a in enumerate(s) if a['op']=='activate')
                resolution=next(i for i,a in enumerate(s) if a['op']=='resolve')
                self.assertEqual(s[activation+1],dict(op='require_activation_mode',value=1))
                self.assertIn(dict(op='require_card',ref='source',property='in_play',value=0),s[activation:resolution])
                self.assertFalse(any(a['op']=='choose_target' and a.get('ref')=='source' for a in s))
                self.assertEqual(self.last(c,'assert_zone_multiplicity',player=owner,zone='graveyard',index=self.rows['Blighted Steppe']['csvid']),1)
                self.assertEqual(self.last(c,'assert_life',player=owner),life)
    def test_held_metrics_parse_complete_typed_clauses_and_keep_target_separate_from_recipient(self):
        door=parse(self.oracle['Doorkeeper'],1);tamiyo=parse(self.oracle['Tamiyo, the Moon Sage'],1);stone=parse(self.oracle["Loreseeker's Stone"],0)
        self.assertEqual(door['effects'][0]['amount'],dict(kind='permanent_count',scope='self',type='Creature',keyword='Defender',multiplier=1))
        self.assertEqual(tamiyo['effects'][0],dict(op='draw',amount=dict(kind='permanent_count',scope='target',type='Creature',tapped=True,multiplier=1),recipient='self'))
        self.assertTrue(tamiyo['target_player'])
        self.assertEqual(stone['derived_cost'],dict(kind='zone_count',scope='self',zone='hand'))
        self.assertEqual(stone['activation_mana'],[3,0,0,0,0,0])
        self.assertFalse(stone['cost']['x'])
        for name,index in [('Doorkeeper',1),('Tamiyo, the Moon Sage',1),("Loreseeker's Stone",0)]:
            lines=self.oracle[name]['oracle_text'].splitlines();lines[index]+=' Then draw a card.'
            with self.assertRaises(ValueError):parse(dict(self.oracle[name],oracle_text='\n'.join(lines)),index)
    def test_defender_count_uses_two_real_card_identities_and_live_own_membership(self):
        for owner in (0,1):
            for mode,library,grave in [('value',10,3),('metric_removed',11,2),('metric_many',8,5),('zero_after_source_removal',12,1)]:
                c=self.case('Doorkeeper',f'{mode}_{owner}',1)
                self.assertEqual(self.last(c,'assert_zone',player=1-owner,zone='library'),library,(mode,owner))
                self.assertEqual(self.last(c,'assert_zone',player=1-owner,zone='graveyard'),grave,(mode,owner))
                self.assertEqual(next(card['name'] for card in c['setup'] if card['ref']=='source'),'Doorkeeper')
                self.assertEqual(next(card['name'] for card in c['setup'] if card['ref']=='enemy_member'),'Wall of Tanglecord')
            c=self.case('Doorkeeper',f'one_short_payment_{owner}',1)
            self.assertEqual(self.last(c,'assert_mana_total',player=owner),2)
            self.assertIn(dict(op='set_mana',player=owner,color=2,value=1),c['steps'])
            self.assertIn(dict(op='require_can_activate',ref='source',value=0),c['steps'])
            c=self.case('Doorkeeper',f'wrong_color_{owner}',1)
            self.assertEqual(self.last(c,'assert_mana_total',player=owner),3)
            self.assertIn(dict(op='set_mana',player=owner,color=2,value=0),c['steps'])
            self.assertIn(dict(op='require_can_activate',ref='source',value=0),c['steps'])
    def test_target_tapped_count_draws_only_for_source_controller_with_asymmetric_sides(self):
        for owner in (0,1):
            opponent=self.case('Tamiyo, the Moon Sage',f'value_{owner}',1)
            own=self.case('Tamiyo, the Moon Sage',f'target_self_{owner}',1)
            self.assertEqual(opponent['setup'],own['setup'])
            for c,hand,target in [(opponent,2,1-owner),(own,3,owner)]:
                self.assertIn(dict(op='choose_player',player=owner,value=target),c['steps'])
                self.assertEqual(self.last(c,'assert_zone',player=owner,zone='hand'),hand)
                self.assertEqual(self.last(c,'assert_zone',player=1-owner,zone='hand'),1)
                self.assertEqual(self.last(c,'assert_counter',ref='source',counter_kind='loyalty'),2)
            zero=self.case('Tamiyo, the Moon Sage',f'no_other_members_{owner}',1)
            self.assertIn(dict(op='activate',ref='source',value=1),zero['steps'])
            self.assertEqual(self.last(zero,'assert_zone',player=owner,zone='hand'),1)
    def test_real_tap_resolves_before_tamiyo_and_removal_occurs_while_parent_is_pending(self):
        for owner in (0,1):
            c=self.case('Tamiyo, the Moon Sage',f'real_tap_before_{owner}',1);s=c['steps']
            icy=next(i for i,a in enumerate(s) if a['op']=='activate' and a['ref']=='metric_tapper')
            source=next(i for i,a in enumerate(s) if a['op']=='activate' and a['ref']=='source')
            self.assertLess(icy,source)
            self.assertIn(dict(op='resolve'),s[icy:source])
            self.assertIn(dict(op='require_card',ref='untapped_member',property='tapped',value=1),s[icy:source])
            self.assertEqual(self.last(c,'require_card',ref='metric_tapper',property='tapped'),1)
            self.assertEqual(self.last(c,'assert_mana_total',player=owner),79)
            self.assertEqual(self.last(c,'assert_zone',player=owner,zone='hand'),3)
            removed=self.case('Tamiyo, the Moon Sage',f'metric_removed_{owner}',1);s=removed['steps']
            source=next(i for i,a in enumerate(s) if a['op']=='activate' and a['ref']=='source');death=next(i for i,a in enumerate(s) if a['op']=='destroy')
            self.assertLess(source,death)
            self.assertIn(dict(op='assert_stack',value=1),s[death:])
            self.assertEqual(self.last(removed,'assert_zone',player=owner,zone='hand'),1)
    def test_own_hand_surcharge_uses_printed_base_before_three_cards_are_drawn(self):
        for owner in (0,1):
            for mode,hand,mana_total,library in [('value',4,76,9),('empty_hand_cost',3,0,9),('three_hand_cost',6,0,9),('short_cost',3,5,12),('cast',4,76,9)]:
                c=self.case("Loreseeker's Stone",f'{mode}_{owner}')
                self.assertEqual(self.last(c,'assert_zone',player=owner,zone='hand'),hand,(mode,owner))
                self.assertEqual(self.last(c,'assert_zone',player=1-owner,zone='hand'),2)
                self.assertEqual(self.last(c,'assert_mana_total',player=owner),mana_total)
                self.assertEqual(self.last(c,'assert_zone',player=owner,zone='library'),library)
                self.assertFalse(any(a['op']=='choose_x' for a in c['steps']))
                if mode=='short_cost':
                    self.assertIn(dict(op='activate',ref='source',value=0),c['steps'])
                    self.assertEqual(self.last(c,'require_card',ref='source',property='tapped'),0)
                if mode=='cast':
                    self.assertIn(dict(op='cast',ref='source',value=1),c['steps'])
                    self.assertIn(dict(op='assert_mana_total',player=owner,value=0),c['steps'])
    def test_new_metric_snapshots_have_independent_receipts_without_state_or_counter_setters(self):
        for name,index in [('Doorkeeper',1),('Tamiyo, the Moon Sage',1),("Loreseeker's Stone",0)]:
            c=self.case(name,'pending_snapshot_1',index);s=c['steps']
            self.assertEqual(len({id(a) for a in s}),len(s))
            self.assertFalse(any(a['op'].startswith(('set_counter','set_tapped','set_keyword')) for a in s))
            self.assertEqual(sum(a['op']=='restore' for a in s),1)
    def test_loyalty_fields_and_unacquired_counter_costs_fail_closed(self):
        for change in (dict(loyalty='*'),dict(loyalty='0')):
            with self.subTest(change=change),self.assertRaises(ValueError):parse(dict(self.oracle['Jace Beleren'],**change),1)
        with self.assertRaises(ValueError):parse(self.oracle['Jace Beleren'],2)
    def test_unreviewed_modal_annotations_cannot_change_presentation_silently(self):
        d=self.declaration('Aether Spellbomb','value_0',1)
        with self.assertRaises(ValueError):spec(dict(d,activation_dialogs={'0':1,'1':1}),self.oracle['Aether Spellbomb'])
        with self.assertRaises(ValueError):spec(dict(d,params=dict(d['params'],present_ai_dialogs='yes')),self.oracle['Aether Spellbomb'])
    def test_typed_sacrifices_keep_contrary_controller_and_type_controls(self):
        c=self.case('Dark Heart of the Wood','value_1')
        self.assertEqual(next(card['name'] for card in c['setup'] if card['ref']=='victim'),'Forest')
        self.assertEqual(self.last(c,'require_card',ref='enemy_eligible',property='in_play'),1)
        self.assertEqual(self.last(c,'require_card',ref='control',property='in_play'),1)
        self.assertEqual(self.last(c,'assert_life',player=1),23)
    def test_saproling_cost_vanishes_instead_of_becoming_a_graveyard_card(self):
        c=self.case('Elvish Farmer','value_0',2)
        self.assertIn('token',next(card['state'] for card in c['setup'] if card['ref']=='victim'))
        self.assertEqual(self.last(c,'assert_life',player=0),22)
        self.assertEqual(self.last(c,'assert_zone',player=0,zone='graveyard'),1)
        self.assertIn(dict(op='assert_matching_permanents',player=1,index=self.rows['Saproling']['csvid'],property='token',value=1),c['steps'])
    def test_target_and_cost_chooser_order_tracks_real_interfaces_without_changing_rules(self):
        for name,ability,first in [('Gnawing Zombie',0,'choose_target'),('Cabal Archon',0,'choose_player')]:
            c=self.case(name,'value_0',ability);choices=[s['op'] for s in c['steps'] if s['op'] in ('choose_target','choose_player')]
            self.assertEqual(choices[0],first)
            self.assertEqual(self.last(c,'assert_life',player=0),21 if name=='Gnawing Zombie' else 22)
            self.assertEqual(self.last(c,'assert_life',player=1),19 if name=='Gnawing Zombie' else 18)
    def test_main_mode_minimum_receipts_bind_intent_and_retain_printed_cost(self):
        for name,ability in [('Heart Warden',1),('Horizon Canopy',1),('Phyrexia\'s Core',1),('Grim Backwoods',1),('Rix Maadi, Dungeon Palace',1),('Memorial to Genius',2)]:
            for owner in (0,1):
                c=self.case(name,f'payment_{owner}',ability);s=c['steps']
                self.assertEqual(self.last(c,'assert_mana_total',player=owner),0)
                self.assertFalse(any(a['op'] in ('choose_dialog','choose_color') for a in s))
                intent=next(i for i,a in enumerate(s) if a['op']=='choose_activation_mode')
                activation=next(i for i,a in enumerate(s) if a['op']=='activate')
                self.assertEqual(s[intent]['player'],owner)
                self.assertLess(intent,activation)
                self.assertEqual(s[activation+1],dict(op='require_activation_mode',value=1))
                self.assertEqual(s[activation+2],dict(op='assert_stack',value=1))
        canopy=self.case('Horizon Canopy','payment_0',1)
        self.assertIn(dict(op='choose_activation_mode',ref='source',player=0,mode_label='Sac and draw a card',fallback_color=3,allow_auto_nonmana=True),canopy['steps'])
        self.assertEqual(self.last(canopy,'require_card',ref='source',property='in_play'),0)
        self.assertEqual(self.last(canopy,'assert_zone',player=0,zone='hand'),2)
    def test_source_indestructibility_is_respected_when_ability_is_pending(self):
        c=self.case('Erebos, God of the Dead','source_destroyed_pending_1',3)
        self.assertEqual(self.last(c,'require_card',ref='source',property='in_play'),1)
        self.assertEqual(self.last(c,'assert_life',player=1),18)
        self.assertEqual(self.last(c,'assert_zone',player=1,zone='hand'),2)
        self.assertFalse(any(s['op']=='require_card' and s.get('ref')=='source' and s.get('property') in ('power','toughness') for s in c['steps']))
    def test_once_per_turn_limit_has_a_second_matching_resource_left(self):
        c=self.case('Aggressive Mining','repeat_limit_1',1)
        self.assertEqual([s['value'] for s in c['steps'] if s['op']=='require_can_activate' and s['ref']=='source'],[1,0])
        self.assertEqual(sum(card['name']=='Forest' and card['player']==1 and card['zone']=='battlefield' for card in c['setup']),2)
        self.assertIn(dict(op='assert_zone_multiplicity',player=1,zone='battlefield',index=self.rows['Forest']['csvid'],value=1),c['steps'])
        self.assertEqual(self.last(c,'assert_zone',player=1,zone='hand'),3)
    def test_source_casting_an_enters_tapped_artifact_does_not_allow_its_tap_cost(self):
        r=dict(self.oracle['Carnage Altar'],oracle_text='This artifact enters tapped.\n{T}: Draw a card.')
        p=parse(r,1);d=dict(family=FAMILY,version=2,oracle_text=r['oracle_text'],params=dict(ability=1),variants=['cast_0'])
        context=dict(self.oracle,**{'Carnage Altar':r});c=expand('Carnage Altar',self.rows['Carnage Altar']['csvid'],d,p,d['variants'],self.rows,context)[0]
        self.assertIn(dict(op='require_can_activate',ref='source',value=0),c['steps'])
        self.assertEqual(self.last(c,'require_card',ref='source',property='tapped'),1)
    def test_enters_tapped_land_ready_fixture_preserves_printed_activation_and_tapped_control(self):
        name='Memorial to Genius';record=self.oracle[name];p=parse(record,2)
        self.assertTrue(p['enters_tapped'])
        for owner in (0,1):
            for mode in ('value','payment'):
                c=self.case(name,f'{mode}_{owner}',2)
                self.assertEqual(next(card['state'] for card in c['setup'] if card['ref']=='source'),['untapped'])
                self.assertIn(dict(op='require_can_activate',ref='source',value=1),c['steps'])
                self.assertEqual(self.last(c,'require_card',ref='source',property='in_play'),0)
                self.assertEqual(self.last(c,'assert_zone',player=owner,zone='hand'),3)
                self.assertEqual(self.last(c,'assert_zone',player=owner,zone='library'),10)
                self.assertEqual(self.last(c,'assert_mana_total',player=owner),0 if mode=='payment' else 75)
            d=self.declaration(name,f'value_{owner}',2)
            c=expand(name,self.rows[name]['csvid'],d,p,[f'tapped_{owner}'],self.rows,self.oracle)[0]
            self.assertEqual(next(card['state'] for card in c['setup'] if card['ref']=='source'),['tapped'])
            self.assertIn(dict(op='require_can_activate',ref='source',value=0),c['steps'])
            self.assertFalse(any(s['op']=='choose_activation_mode' for s in c['steps']))
    def test_dark_heart_ai_sacrifice_is_automatic_while_overgrown_legacy_picker_is_explicit(self):
        dark=self.case('Dark Heart of the Wood','value_1');estate=self.case('Overgrown Estate','value_1')
        self.assertFalse(any(s['op']=='choose_target' and s['player']==1 for s in dark['steps']))
        self.assertIn(dict(op='choose_target',player=1,ref='victim'),estate['steps'])
        for c in (dark,estate):
            self.assertEqual(self.last(c,'assert_life',player=1),23)
            self.assertEqual(self.last(c,'assert_zone_multiplicity',player=1,zone='graveyard',index=self.rows['Forest']['csvid']),1)
    def test_enters_tapped_artifact_ready_fixture_keeps_both_real_elixir_costs_and_effects(self):
        name='Elixir of Vitality'
        for owner in (0,1):
            for ability,gain,payment in ((1,4,0),(2,8,8)):
                for mode in ('value','payment'):
                    c=self.case(name,f'{mode}_{owner}',ability)
                    self.assertEqual(next(card['state'] for card in c['setup'] if card['ref']=='source'),['untapped'])
                    self.assertEqual(next(card['state'] for card in c['setup'] if card['ref']=='enemy_eligible'),['untapped'])
                    self.assertEqual(self.last(c,'require_card',ref='enemy_eligible',property='tapped'),0)
                    self.assertIn(dict(op='require_can_activate',ref='source',value=1),c['steps'])
                    self.assertEqual(self.last(c,'assert_life',player=owner),20+gain)
                    self.assertEqual(self.last(c,'assert_mana_total',player=owner),0 if mode=='payment' else 80-payment)
                    self.assertEqual(self.last(c,'require_card',ref='source',property='in_play'),0)
                d=self.declaration(name,f'value_{owner}',ability);p=spec(d,self.oracle[name])
                c=expand(name,self.rows[name]['csvid'],d,p,[f'tapped_{owner}'],self.rows,self.oracle)[0]
                self.assertEqual(next(card['state'] for card in c['setup'] if card['ref']=='source'),['tapped'])
                self.assertEqual(next(card['state'] for card in c['setup'] if card['ref']=='enemy_eligible'),['untapped'])
                self.assertIn(dict(op='require_can_activate',ref='source',value=0),c['steps'])
    def test_enters_tapped_recognition_requires_an_exact_unconditional_clause(self):
        for typ in ('artifact','creature','enchantment','land','planeswalker'):
            walker=typ=='planeswalker';cost='+1' if walker else '{T}'
            base=dict(self.oracle['Sage of Lat-Nam'],type_line=typ.title(),loyalty='3')
            for clause,expected in ((f'This {typ} enters tapped.',True),(f'This {typ} enters tapped unless you control a Forest.',False)):
                record=dict(base,oracle_text=clause+'\n'+cost+': Draw a card.')
                with self.subTest(typ=typ,clause=clause):self.assertEqual(parse(record,1)['enters_tapped'],expected)
    def test_value_intent_is_followed_by_only_conditional_cost_choices_then_immediate_receipt(self):
        c=self.case('Grim Backwoods','value_0',1);s=c['steps']
        intent=next(i for i,a in enumerate(s) if a['op']=='choose_activation_mode');activation=next(i for i,a in enumerate(s) if a['op']=='activate')
        self.assertEqual(s[intent+1:activation],[dict(op='choose_target',player=0,ref='victim')])
        self.assertEqual(s[activation+1],dict(op='require_activation_mode',value=1))
    def test_empty_selected_discard_mode_is_rejected_while_sibling_permission_remains_legal(self):
        for name in ('Compulsion','Dismissive Pyromancer','Trading Post'):
            for owner in (0,1):
                c=self.case(name,f'bare_zones_{owner}');s=c['steps']
                start=next(i for i,a in enumerate(s) if a['op']=='require_can_activate')
                self.assertEqual(s[start:start+3],[dict(op='require_can_activate',ref='source',value=1),
                                 dict(op='choose_dialog',player=owner,value=0),dict(op='activate',ref='source',value=0)])
                self.assertEqual(self.last(c,'assert_life',player=owner),20)
                self.assertEqual(self.last(c,'assert_mana_total',player=owner),80)
                self.assertEqual(self.last(c,'assert_zone',player=owner,zone='hand'),0)
                self.assertEqual(self.last(c,'require_card',ref='source',property='in_play'),1)
                self.assertEqual(self.last(c,'require_card',ref='source',property='tapped'),0)
                self.assertEqual(self.last(c,'assert_stack'),0)
    def test_one_legal_mode_autochooses_only_for_human_while_raw_elixir_threshold_stays_real(self):
        for name,ability,choice in (('Arcanis the Omnipotent',0,0),('Zacama, Primal Calamity',4,2)):
            for owner in (0,1):
                c=self.case(name,f'payment_{owner}',ability)
                self.assertEqual([s for s in c['steps'] if s['op']=='choose_dialog'],
                                 [] if owner==0 else [dict(op='choose_dialog',player=owner,value=choice)])
                self.assertEqual(self.last(c,'assert_mana_total',player=owner),0)
        for owner in (0,1):
            low=self.case('Elixir of Vitality',f'payment_{owner}',1)
            high=self.case('Elixir of Vitality',f'payment_{owner}',2)
            self.assertFalse(any(s['op']=='choose_dialog' for s in low['steps']))
            self.assertIn(dict(op='choose_dialog',player=owner,value=1),high['steps'])
    def test_auto_value_intent_cannot_hide_multiple_nonmana_abilities_or_unreviewed_label(self):
        for name,index in [('Compulsion',0),('Elixir of Vitality',1),('Icy Manipulator',0)]:
            with self.subTest(name=name),self.assertRaises(ValueError):validate_unique_value_mode(self.oracle[name],index)
        d=self.declaration('Heart Warden','payment_0',1)
        for bad in (dict(mode_label='Get mana'),dict(fallback_color=7),dict(allow_auto_nonmana=False),dict(allow_auto_nonmana=1),dict(mode_label='Bad\nlabel')):
            changed=copy.deepcopy(d);changed['params']['mode_intent'].update(bad)
            with self.subTest(bad=bad),self.assertRaises(ValueError):spec(changed,self.oracle['Heart Warden'])
        changed=copy.deepcopy(d);changed['params']['mode_dialog_minimum']=3
        with self.assertRaises(ValueError):spec(changed,self.oracle['Heart Warden'])
    def test_heart_warden_ai_still_requires_the_oracle_draw_after_mode_selection(self):
        c=self.case('Heart Warden','value_1',1)
        self.assertIn(dict(op='require_activation_mode',value=1),c['steps'])
        self.assertEqual(self.last(c,'assert_zone',player=1,zone='hand'),2)
        self.assertEqual(self.last(c,'assert_zone',player=1,zone='library'),11)
        self.assertEqual(self.last(c,'require_card',ref='source',property='in_play'),0)
    def test_elixir_legitimate_eight_mana_sibling_visibility_stays_unchanged(self):
        c=self.case('Elixir of Vitality','payment_0',1)
        self.assertFalse(any(a['op'] in ('choose_activation_mode','choose_dialog') for a in c['steps']))
        self.assertEqual(self.last(c,'assert_life',player=0),24)
        self.assertEqual(self.last(c,'assert_mana_total',player=0),0)

if __name__=='__main__':unittest.main()
