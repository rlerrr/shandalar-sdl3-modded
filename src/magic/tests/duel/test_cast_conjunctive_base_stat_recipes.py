import copy,json,unittest
from pathlib import Path
from generate_cases import registry
import cast_conjunctive_base_stat_recipes as f

class ConjunctionTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.o=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards'];cls.rows={r['name']:r for r in registry()}
    def case(self,n='Battlegate Mimic',v='matching_spell_0'):
        r=self.o[n];p=f.classify(r);d=dict(family=f.FAMILY,version=2,oracle_text=r['oracle_text'],params=f.parameters(p),variants=[v]);return f.expand(n,self.rows[n]['csvid'],d,f.spec(d,r),[v],self.rows,self.o)[0]
    def final(self,c,op,**kw):return next(x['value'] for x in reversed(c['steps']) if x['op']==op and all(x.get(k)==v for k,v in kw.items()))
    def stat(self,c,key,ref='source'):return self.final(c,'assert_card',ref=ref,property=key)
    def mult(self,c,n,w,z):return self.final(c,'assert_zone_multiplicity',player=w,zone=z,index=self.rows[n]['csvid'])
    def test_complete_name_neutral_Oracle_and_matching_hybrid_pair(self):
        self.assertEqual(len(f.pins()['sources']),5);f.validate_witnesses(self.o,self.rows)
        for n in f.pins()['sources']:
            r=self.o[n];p=f.classify(r);self.assertEqual(f.parameters(p),f.parameters(f.classify(dict(r,name='Independent creature'))));self.assertEqual(set(p['colors']),set(p['hybrid_pair']))
            for changed in (dict(r,layout='transform'),dict(r,oracle_text=r['oracle_text']+'\nDraw a card.'),dict(r,oracle_text=r['oracle_text'].replace('both ','either ')),dict(r,mana_cost='{1}{W/W}')):
                with self.assertRaises(ValueError):f.classify(changed)
    def test_full_Wither_reminder_and_numeric_surviving_base_fail_closed(self):
        r=self.o['Woodlurker Mimic']
        for text in (r['oracle_text'].split(' (')[0],r['oracle_text'].replace('4/5','4/0'),r['oracle_text'].replace('wither','infect')):
            with self.assertRaises(ValueError):f.classify(dict(r,oracle_text=text))
    def test_typed_parameters_and_source_pin_cannot_be_normalized(self):
        n='Nightsky Mimic';r=self.o[n];p=f.classify(r);d=dict(family=f.FAMILY,version=2,oracle_text=r['oracle_text'],params=f.parameters(p),variants=['matching_spell_0'])
        for k,v in [('base_power',True),('colors',['W']),('grant','lifelink'),('hybrid_pair',['W','G']),('extra',False)]:
            q=copy.deepcopy(d);q['params'][k]=v
            with self.assertRaises(ValueError):f.spec(q,r)
    def test_witness_whole_Oracle_and_handler_pins(self):
        for n in ('Mourning Thrull','Crag Puca','Travel Preparations','Fusion Elemental'):
            o=copy.deepcopy(self.o);o[n]['oracle_text']+='\nDraw a card.'
            with self.assertRaises(ValueError):f.validate_witnesses(o,self.rows)
        rows=copy.deepcopy(self.rows);rows['Riverfall Mimic']['handler']='card_dummy'
        with self.assertRaises(ValueError):f.validate_witnesses(self.o,rows)
        self.assertEqual(f.pins()['witnesses']['Wall of Denial']['power'],'0');self.assertEqual(f.pins()['witnesses']['Wall of Denial']['toughness'],'8')
    def test_each_mono_spell_and_separate_colors_do_not_form_a_conjunction(self):
        for n in f.pins()['sources']:
            for who in (0,1):
                for mode in ('first_color','second_color','separate_colors','unrelated_multicolor','colorless_spell'):
                    c=self.case(n,f'{mode}_{who}');self.assertEqual(self.stat(c,'power'),2);self.assertEqual(self.stat(c,'toughness'),1)
    def test_dual_color_positive_and_additional_colors_are_inclusive(self):
        for n in f.pins()['sources']:
            p=f.classify(self.o[n])
            for who in (0,1):
                for mode in ('matching_spell','extra_colors','same_card_spell','same_card_other_payment'):
                    c=self.case(n,f'{mode}_{who}');self.assertEqual(self.stat(c,'power'),p['base_power']);self.assertEqual(self.stat(c,'toughness'),p['base_toughness'])
                c=self.case(n,f'extra_colors_{who}');self.assertTrue(any(x['name']=='Fusion Elemental' and x['zone']=='hand' for x in c['setup']))
    def test_real_spell_announcement_precedes_value_receipts_and_actual_resolution(self):
        c=self.case();s=c['steps'];i=next(i for i,x in enumerate(s) if x['op']=='announce_spell');j=next(i for i,x in enumerate(s) if x['op']=='resolve');self.assertLess(i,j)
        self.assertIn(dict(op='assert_card',ref='source',property='power',value=4),s[i:j]);self.assertIn(dict(op='require_card',ref='witness_0',property='in_hand',value=0),s[:i])
    def test_source_cast_and_late_entry_do_not_self_or_retro_trigger(self):
        for n in f.pins()['sources']:
            for who in (0,1):
                for mode in ('source_cast','source_arrives_late','bounce_recast'):
                    c=self.case(n,f'{mode}_{who}');self.assertEqual(self.stat(c,'power'),2);self.assertEqual(self.stat(c,'toughness'),1)
    def test_opposing_caster_is_not_the_controllers_trigger(self):
        for n in f.pins()['sources']:
            c=self.case(n,'opponent_cast_1');self.assertEqual(self.stat(c,'power'),2);self.assertEqual(self.stat(c,'toughness'),1)
    def test_hybrid_both_payment_alternatives_exact_pool_and_no_AI_preference(self):
        for n in f.pins()['sources']:
            for who in (0,1):
                for mode in ('source_payment','source_other_payment','source_generic_payment'):
                    c=self.case(n,f'{mode}_{who}');self.assertEqual(self.final(c,'assert_mana_total',player=who),0);self.assertFalse(any(x['op']=='choose_payment' for x in c['steps']))
                c=self.case(n,f'same_card_other_payment_{who}');self.assertEqual(self.stat(c,'power'),f.classify(self.o[n])['base_power']);self.assertEqual(self.stat(c,'power','witness_0'),2)
    def test_human_generic_colored_pool_has_one_real_mana_input_and_AI_is_unchanged(self):
        colors={'Battlegate Mimic':1,'Nightsky Mimic':2,'Riverfall Mimic':1,'Shorecrasher Mimic':1,'Woodlurker Mimic':2}
        for n,color in colors.items():
            p=f.classify(self.o[n]);self.assertNotIn('CBUGRW'[color],p['hybrid_pair'])
            c=self.case(n,'source_generic_payment_0');s=c['steps'];i=next(i for i,x in enumerate(s) if x['op']=='cast' and x['ref']=='source')
            self.assertEqual(s[i-1],dict(op='choose_mana',player=0,color=color));self.assertEqual(sum(x['op']=='choose_mana' for x in s),1);self.assertEqual(self.final(c,'assert_mana_total',player=0),0)
            self.assertFalse(any(x['op']=='choose_mana' for x in self.case(n,'source_generic_payment_1')['steps']))
    def test_hybrid_wrong_color_excludes_both_legal_alternatives(self):
        for n in f.pins()['sources']:
            p=f.classify(self.o[n]);c=self.case(n,'source_wrong_color_0');self.assertIn(dict(op='cast',ref='source',value=0),c['steps'])
            for color in p['hybrid_pair']:self.assertEqual(self.final(c,'assert_mana',player=0,color='CBUGRW'.index(color)),0)
            self.assertEqual(self.final(c,'assert_mana_total',player=0),2)
    def test_real_Growth_before_and_after_base_layer_yield_same_final_addition(self):
        for n in f.pins()['sources']:
            p=f.classify(self.o[n])
            for mode in ('growth_before','growth_after','repeat_after_growth'):
                c=self.case(n,mode+'_0');self.assertEqual(self.stat(c,'power'),p['base_power']+3);self.assertEqual(self.stat(c,'toughness'),p['base_toughness']+3);self.assertTrue(any(x['name']=='Giant Growth' for x in c['setup']))
    def test_real_two_target_counter_spell_before_after_and_combined_layers(self):
        for n in f.pins()['sources']:
            p=f.classify(self.o[n])
            for mode in ('counter_before','counter_after','growth_counter_before','growth_counter_after'):
                c=self.case(n,mode+'_1');extra=4 if mode.startswith('growth') else 1;self.assertEqual(self.stat(c,'power'),p['base_power']+extra);self.assertEqual(self.stat(c,'toughness'),p['base_toughness']+extra);self.assertEqual(self.stat(c,'plus1_counters'),1);self.assertEqual(self.stat(c,'plus1_counters','idle'),1);self.assertEqual(self.stat(c,'power','idle'),3)
                self.assertIn(dict(op='choose_target',player=1,ref='source'),c['steps']);self.assertIn(dict(op='choose_target',player=1,ref='idle'),c['steps'])
    def test_cleanup_removes_base_setting_but_preserves_permanent_counters(self):
        for n in f.pins()['sources']:
            c=self.case(n,'counter_cleanup_0');self.assertEqual(self.stat(c,'power'),3);self.assertEqual(self.stat(c,'toughness'),2);self.assertEqual(self.stat(c,'plus1_counters'),1);self.assertEqual(self.stat(c,'power','idle'),3);self.assertIn(dict(op='cleanup'),c['steps'])
    def test_repeated_matching_spells_do_not_stack_set_base_values(self):
        for n in f.pins()['sources']:
            c=self.case(n,'two_casts_1');self.assertEqual(self.stat(c,'power'),f.classify(self.o[n])['base_power']);self.assertEqual(sum(x['op']=='announce_spell' for x in c['steps']),2)
    def test_matching_countered_spell_still_changes_base_and_untouched_source_spell_dies(self):
        for n in f.pins()['sources']:
            c=self.case(n,'countered_witness_0');self.assertEqual(self.stat(c,'power'),f.classify(self.o[n])['base_power']);self.assertIn(dict(op='cast_response',ref='response',value=1),c['steps']);self.assertIn(dict(op='resolve_response'),c['steps'])
            c=self.case(n,'countered_source_1');self.assertEqual(self.mult(c,n,1,'graveyard'),1)
    def test_two_source_mandatory_ordering_and_both_results(self):
        for n in f.pins()['sources']:
            c=self.case(n,'two_sources_0');self.assertEqual([x for x in c['steps'] if x['op']=='choose_trigger_source'],[dict(op='choose_trigger_source',player=0,ref='source')]);self.assertEqual(self.stat(c,'power'),f.classify(self.o[n])['base_power']);self.assertEqual(self.stat(c,'power','second'),f.classify(self.o[n])['base_power'])
    def test_destroyed_source_receipts_use_grave_zones_not_reused_live_instances(self):
        for n in f.pins()['sources']:
            for mode in ('source_removed_before','source_removed_after'):
                c=self.case(n,mode+'_0');s=c['steps'];i=next(i for i,x in enumerate(s) if x['op']=='destroy');self.assertFalse(any(x['op']=='assert_card' and x.get('ref')=='source' for x in s[i:]));self.assertEqual(self.mult(c,n,0,'graveyard'),1)
    def test_bounce_rebinds_actual_hand_and_recast_is_new_unmodified_object(self):
        for n in f.pins()['sources']:
            c=self.case(n,'bounce_recast_1');self.assertIn(dict(op='bind_hand_copy',player=1,value=self.rows[n]['csvid'],index=0,ref='source'),c['steps']);self.assertEqual(self.stat(c,'power'),2);self.assertEqual(self.stat(c,'plus1_counters'),0)
    def test_fresh_source_cannot_attack_and_has_no_self_grant(self):
        for n in f.pins()['sources']:
            c=self.case(n,'fresh_attack_0');self.assertIn(dict(op='attack',ref='source',value=0),c['steps']);self.assertEqual(self.stat(c,'tapped'),0);self.assertEqual(self.stat(c,'power'),2)
    def test_first_strike_kills_before_normal_damage(self):
        c=self.case('Battlegate Mimic','blocked_ground_0');self.assertEqual(self.stat(c,'damage'),0);self.assertEqual(self.mult(c,'Grizzly Bears',1,'graveyard'),2);self.assertEqual(self.mult(c,'Battlegate Mimic',0,'graveyard'),0)
    def test_Flying_and_unblockable_have_real_block_rejections(self):
        for n in ('Nightsky Mimic','Riverfall Mimic'):
            c=self.case(n,'blocked_ground_0');self.assertIn(dict(op='block',ref='blocker',other='source',value=0),c['steps'])
        self.assertIn(dict(op='block',ref='blocker',other='source',value=1),self.case('Nightsky Mimic','blocked_reach_1')['steps'])
        self.assertIn(dict(op='block',ref='blocker',other='source',value=0),self.case('Riverfall Mimic','blocked_reach_1')['steps'])
    def test_Trample_assigns_lethal_and_excess_without_invented_damage(self):
        c=self.case('Shorecrasher Mimic','blocked_ground_0');self.assertEqual(self.final(c,'assert_life',player=1),17);self.assertEqual(self.stat(c,'damage'),2)
        c=self.case('Shorecrasher Mimic','blocked_large_1');self.assertEqual(self.final(c,'assert_life',player=0),20);self.assertEqual(self.stat(c,'damage','blocker'),5)
    def test_Wither_uses_persistent_minus_counters_not_marked_damage_or_poison(self):
        c=self.case('Woodlurker Mimic','blocked_large_0');self.assertEqual(self.stat(c,'minus1_counters','blocker'),4);self.assertEqual(self.stat(c,'damage','blocker'),0);self.assertEqual(self.stat(c,'toughness','blocker'),4)
        c=self.case('Woodlurker Mimic','unblocked_1');self.assertEqual(self.final(c,'assert_life',player=0),16);self.assertEqual(self.final(c,'assert_poison',player=0),0)
    def test_combat_cleanup_clears_marked_damage_at_same_time_as_shrinking_base(self):
        for n in f.pins()['sources']:
            c=self.case(n,'combat_cleanup_0');self.assertEqual(self.stat(c,'power'),2);self.assertEqual(self.stat(c,'toughness'),1);self.assertEqual(self.stat(c,'damage'),0);self.assertEqual(self.mult(c,n,0,'graveyard'),0)
    def test_all_temporary_grants_expire_before_actual_later_combat(self):
        for n in f.pins()['sources']:
            c=self.case(n,'expiry_combat_1');s=c['steps'];self.assertIn(dict(op='cleanup'),s);self.assertEqual(sum(x['op']=='advance_turn' for x in s),2);self.assertIn(dict(op='untap_step',player=1),s);self.assertIn(dict(op='block',ref='blocker',other='source',value=1),s)
        c=self.case('Woodlurker Mimic','expiry_combat_0');self.assertEqual(self.stat(c,'damage','blocker'),2);self.assertEqual(self.stat(c,'minus1_counters','blocker'),0)
    def test_snapshots_cover_ready_pending_combat_and_resolved_boundaries(self):
        for n in f.pins()['sources']:
            for mode in ('ready_snapshot','pending_snapshot','combat_snapshot','resolved_snapshot'):
                c=self.case(n,mode+'_1');self.assertIn(dict(op='capture'),c['steps']);self.assertIn(dict(op='assert_recorded_state'),c['steps'])
    def test_Wither_counters_survive_actual_cleanup_while_normal_damage_clears(self):
        for who in (0,1):
            c=self.case('Woodlurker Mimic',f'large_combat_cleanup_{who}');self.assertEqual(self.stat(c,'minus1_counters','blocker'),4);self.assertEqual(self.stat(c,'toughness','blocker'),4);self.assertEqual(self.stat(c,'damage','blocker'),0);self.assertEqual(self.stat(c,'power'),2);self.assertEqual(self.stat(c,'toughness'),1)
            c=self.case('Battlegate Mimic',f'large_combat_cleanup_{who}');self.assertEqual(self.stat(c,'minus1_counters','blocker'),0);self.assertEqual(self.stat(c,'toughness','blocker'),8);self.assertEqual(self.stat(c,'damage','blocker'),0)
    def test_all480_full_cases_are_bounded_real_action_sequences(self):
        total=0
        for n in f.pins()['sources']:
            p=f.classify(self.o[n]);vs=f.variants(p);self.assertEqual(len(vs),len(set(vs)));total+=len(vs)
            for v in vs:
                c=self.case(n,v);self.assertEqual(c['_recipe']['oracle_text'],self.o[n]['oracle_text']);self.assertLess(len(c['steps']),12000);self.assertFalse(any(x['op'] in ('set_power','set_counter','add_counter','set_history','set_storm','apply_keyword') for x in c['steps']))
        self.assertEqual(total,480)

if __name__=='__main__':unittest.main()
