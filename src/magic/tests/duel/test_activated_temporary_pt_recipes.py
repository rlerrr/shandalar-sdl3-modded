"""Independent signed stats, real costs, duration and combat contract checks."""
import copy
import json
from pathlib import Path
import unittest
from generate_cases import registry
import activated_temporary_pt_recipes as model


class ActivatedTemporaryPTRecipesTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows={r['name']:r for r in registry()}

    def declaration(self,nm,variant='value_0',ability=None):
        s=model.pins()['sources'][nm]
        if ability is None:ability=int(next(iter(s['abilities'])))
        ui=s.get('controller_interfaces',{}).get(str(ability),{}).get(variant.rsplit('_',1)[-1],s['abilities'][str(ability)])
        ui=s.get('variant_interfaces',{}).get(str(ability),{}).get(variant,ui)
        return dict(family=model.FAMILY,version=2,oracle_text=self.oracle[nm]['oracle_text'],params=dict(ability=ability,interface=copy.deepcopy(ui),other_clauses=[v for i,v in enumerate(self.oracle[nm]['oracle_text'].splitlines()) if i!=ability]),variants=[variant])

    def case(self,nm,v='value_0',a=None):
        d=self.declaration(nm,v,a);p=model.spec(d,self.oracle[nm]);return model.expand(nm,self.rows[nm]['csvid'],d,p,[v],self.rows,self.oracle)[0]

    def last(self,c,op,**fields):return [s for s in c['steps'] if s['op']==op and all(s.get(k)==v for k,v in fields.items())][-1]['value']
    def pending(self,c):
        s=c['steps'];a=next(i for i,v in enumerate(s) if v['op']=='activate' and v['ref']=='source');b=next(i for i in range(a+1,len(s)) if s[i]['op']=='resolve');return s[a:b]

    def test_fiftyfive_implemented_normal_primaries_sixtyfive_profiles(self):
        sources=model.pins()['sources'];self.assertEqual(len(sources),55);self.assertEqual(sum(len(s['abilities']) for s in sources.values()),65)
        for nm,s in sources.items():
            self.assertEqual(self.rows[nm]['kind'],'card');self.assertNotIn('dummy',self.rows[nm]['handler']);self.assertEqual(s['csvid'],self.rows[nm]['csvid']);self.assertEqual(s['oracle']['layout'],'normal')

    def test_full_clause_derives_signed_deltas_scope_and_cost(self):
        p=model.parse(self.oracle['Watercourser'],0);self.assertEqual((p['power'],p['toughness'],p['scope']),(1,-1,'self'));self.assertEqual(p['activation_mana'],[0,0,1,0,0,0])
        p=model.parse(self.oracle['Cabal Patriarch'],0);self.assertEqual((p['power'],p['toughness'],p['scope']),(-2,-2,'target'));self.assertEqual(p['cost']['sacrifice'],'creature')

    def test_char_rumbler_has_independent_negative_base_and_double_strike_not_alias_flying(self):
        c=self.case('Char-Rumbler');pending=self.pending(c)
        self.assertEqual(self.oracle['Char-Rumbler']['power'],'-1');self.assertEqual(self.rows['Char-Rumbler']['handler'],'manalinkeh_card_furnace_whelp')
        self.assertIn(dict(op='require_card',ref='source',property='power',value=-1),pending)
        self.assertEqual(self.last(c,'require_card',ref='source',property='power'),0)
        self.assertNotIn('Flying',self.oracle['Char-Rumbler']['keywords'])

    def test_temporary_changes_are_not_permanent_counters(self):
        c=self.case('Watercourser')
        self.assertEqual(self.last(c,'require_card',ref='source',property='power'),3);self.assertEqual(self.last(c,'require_card',ref='source',property='toughness'),2)
        self.assertEqual(self.last(c,'assert_counter',ref='source',counter_kind='plus1'),0)

    def test_pending_mana_paid_but_stats_unchanged_until_resolve(self):
        c=self.case('Watercourser','payment_0');p=self.pending(c)
        self.assertIn(dict(op='require_card',ref='source',property='power',value=2),p)
        self.assertIn(dict(op='assert_mana_total',player=0,value=0),p)

    def test_cleanup_restores_base_and_real_counter_survives(self):
        c=self.case('Watercourser','counter_cleanup_0')
        self.assertIn(dict(op='cast',ref='counter_spell',value=1),c['steps'])
        self.assertEqual(self.last(c,'assert_counter',ref='source',counter_kind='plus1'),1)
        self.assertEqual(self.last(c,'require_card',ref='source',property='power'),3);self.assertEqual(self.last(c,'require_card',ref='source',property='toughness'),4)

    def test_repeated_negative_toughness_dies_via_real_sba_refresh(self):
        c=self.case('Watercourser','lethal_repeat_0')
        self.assertEqual(sum(s['op']=='activate' and s['ref']=='source' for s in c['steps']),3)
        self.assertEqual(self.last(c,'assert_zone_multiplicity',player=0,zone='battlefield',index=12639),0)
        self.assertEqual(self.last(c,'assert_zone_multiplicity',player=0,zone='graveyard',index=12639),1)
        self.assertTrue(any(c['steps'][i+1]['op']=='refresh' for i,s in enumerate(c['steps'][:-1]) if s['op']=='resolve'))

    def test_armorer_real_labels_have_no_pending_auto_mode_assumption(self):
        for a,label in [(0,'Pump power'),(1,'Pump toughness')]:
            c=self.case('Armorer Guildmage','payment_1',a);s=c['steps'];i=next(i for i,x in enumerate(s) if x['op']=='activate' and x['ref']=='source')
            self.assertIn(dict(op='choose_activation_mode',ref='source',player=1,mode_label=label,fallback_color=0,allow_auto_nonmana=False),s)
            self.assertEqual(s[i+1],dict(op='require_activation_mode',value=1))

    def test_fresh_tapped_and_repeat_tap_denials_preserve_other_mode_rules(self):
        for v in ('fresh_0','tapped_1','repeat_0'):
            c=self.case('Armorer Guildmage',v,0);self.assertEqual(self.last(c,'require_can_activate',ref='source'),0)
        self.assertEqual(self.last(self.case('Watercourser','fresh_0'),'require_can_activate',ref='source'),1)

    def test_printed_other_color_can_keep_global_permission_without_selected_action(self):
        for a,color in [(0,3),(1,1)]:
            c=self.case('Armorer Guildmage','other_mode_only_1',a)
            self.assertEqual(self.last(c,'require_can_activate',ref='source'),1)
            self.assertFalse(any(s['op']=='activate' and s['ref']=='source' for s in c['steps']))
            self.assertEqual(self.last(c,'assert_mana_total',player=1),1)

    def test_discard_uses_target_then_real_hand_choice_and_cost_before_effect(self):
        c=self.case('Deepwood Drummer','payment_0');s=c['steps'];i=next(i for i,x in enumerate(s) if x['op']=='activate' and x['ref']=='source')
        choices=[x['op'] for x in s[:i] if x['op'].startswith('choose_')]
        self.assertEqual(choices[-2:],['choose_target','choose_zone_card'])
        self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='graveyard',index=self.rows['Plains']['csvid'],value=1),self.pending(c))
        self.assertIn(dict(op='require_card',ref='ally',property='power',value=1),self.pending(c))
        self.assertEqual(self.last(c,'require_card',ref='ally',property='power'),3)

    def test_empty_hand_is_a_true_discard_cost_denial(self):
        c=self.case('Deepwood Drummer','empty_hand_1');self.assertEqual(self.last(c,'require_can_activate',ref='source'),0)
        self.assertEqual(self.last(c,'assert_zone',player=1,zone='hand'),0)

    def test_repeat_discard_has_identical_disposables_and_other_player_real_untap(self):
        c=self.case('Deepwood Drummer','repeat_untap_1')
        self.assertEqual(next(x for x in c['setup'] if x['ref']=='untap')['player'],0)
        self.assertEqual(sum(x['name']=='Plains' and x['player']==1 and x['zone']=='hand' for x in c['setup']),2)
        self.assertEqual(self.last(c,'assert_zone_multiplicity',player=1,zone='graveyard',index=self.rows['Plains']['csvid']),2)

    def test_source_can_be_legal_creature_sacrifice_and_has_no_self_effect_when_gone(self):
        for owner in (0,1):
            c=self.case('Bloodthrone Vampire',f'sacrifice_source_{owner}')
            own=[x for x in c['setup'] if x['player']==owner and x['zone']=='battlefield' and 'Creature' in self.oracle[x['name']]['type_line']]
            self.assertEqual(own,[])
            self.assertEqual(self.last(c,'assert_zone_multiplicity',player=owner,zone='graveyard',index=10880),1)
            self.assertEqual(self.last(c,'assert_zone_multiplicity',player=owner,zone='battlefield',index=10880),0)

    def test_sacrifice_self_enchantment_leaves_before_external_effect_and_counter_cleanup(self):
        c=self.case('Seal of Strength','counter_cleanup_0');p=self.pending(c)
        self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='graveyard',index=4759,value=1),p)
        self.assertIn(dict(op='require_card',ref='ally',property='power',value=2),p)
        self.assertEqual(self.last(c,'assert_counter',ref='ally',counter_kind='plus1'),1)
        self.assertEqual(self.last(c,'require_card',ref='ally',property='power'),2)

    def test_cabal_ui_sacrifice_precedes_target_and_cost_is_independent_of_effect(self):
        c=self.case('Cabal Patriarch','payment_0');s=c['steps'];i=next(i for i,x in enumerate(s) if x['op']=='activate' and x['ref']=='source')
        self.assertEqual([x['ref'] for x in s[:i] if x['op']=='choose_target'][-2:],['victim0','ally'])
        self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='graveyard',index=514,value=1),self.pending(c))
        self.assertEqual(self.last(c,'require_card',ref='ally',property='power'),-1)

    def test_any_target_accepts_enemy_and_source_removal_does_not_erase_external_modifier(self):
        c=self.case('Sacred Armory','target_enemy_1');self.assertEqual(self.last(c,'require_card',ref='enemy',property='power'),2)
        c=self.case('Sacred Armory','source_removed_pending_0');self.assertEqual(self.last(c,'require_card',ref='ally',property='power'),2)
        self.assertEqual(self.last(c,'assert_zone_multiplicity',player=0,zone='graveyard',index=14165),1)

    def test_target_removed_pending_receives_no_modifier(self):
        c=self.case('Sacred Armory','target_removed_pending_1')
        self.assertEqual(self.last(c,'assert_zone_multiplicity',player=1,zone='graveyard',index=1705),1)

    def test_actual_double_strike_combat_zero_and_positive_not_a_fake_keyword_receipt(self):
        for v,life in [('combat_zero_0',20),('combat_positive_0',18)]:
            c=self.case('Char-Rumbler',v);self.assertIn(dict(op='combat_damage'),c['steps']);self.assertEqual(self.last(c,'assert_life',player=1),life)

    def test_pending_snapshot_captures_costs_and_replays_real_effect(self):
        c=self.case('Armorer Guildmage','pending_snapshot_1',1);s=c['steps'];cap=next(i for i,x in enumerate(s) if x['op']=='capture')
        self.assertTrue(any(x['op']=='activate' and x['ref']=='source' for x in s[:cap]))
        self.assertFalse(any(x['op']=='resolve' for x in s[max(i for i,x in enumerate(s[:cap]) if x['op']=='activate'):cap]))
        self.assertTrue(any(x['op']=='assert_recorded_state' for x in s))

    def test_schemas_fail_closed_and_witnesses_are_independently_pinned(self):
        n='Watercourser'
        for k,v in [('family','permanent_plus1_activation'),('version',True),('variants',[]),('variants',['value_0','value_0']),('variants',['unknown_0']),('variants',[1])]:
            d=self.declaration(n);d[k]=v
            with self.assertRaises(ValueError):model.spec(d,self.oracle[n])
        d=self.declaration(n);d['params']['interface']['extra']=True
        with self.assertRaises(ValueError):model.spec(d,self.oracle[n])
        changed=copy.deepcopy(self.oracle);changed['Bottle Gnomes']['toughness']='4';d=self.declaration(n);p=model.spec(d,self.oracle[n])
        with self.assertRaises(ValueError):model.expand(n,12639,d,p,['value_0'],self.rows,changed)

    def test_no_state_setters_and_all_future_entities_predeclared(self):
        for n in model.pins()['sources']:
            c=self.case(n);self.assertFalse(any(s['op'] in ('set_power','set_toughness','set_counter','add_counter') for s in c['steps']))
            self.assertTrue(all('state' in x for x in c['setup'] if x['zone']=='battlefield'))

    def test_psychatog_human_single_auto_then_real_grave_threshold_menu_and_ai_menu(self):
        c=self.case('Psychatog','value_0')
        self.assertFalse(any(s['op']=='choose_activation_mode' for s in c['steps']))
        for v in ('repeat_0','value_1'):
            c=self.case('Psychatog',v);menus=[s for s in c['steps'] if s['op']=='choose_activation_mode']
            self.assertEqual(len(menus),1);self.assertEqual(menus[0]['mode_label'],'Discard');self.assertFalse(menus[0]['allow_auto_nonmana'])
            self.assertEqual(self.last(c,'require_card',ref='source',property='power'),3 if v=='repeat_0' else 2)
            self.assertEqual(self.last(c,'assert_zone_multiplicity',player=int(v[-1]),zone='graveyard',index=self.rows['Plains']['csvid']),2 if v=='repeat_0' else 1)

    def test_unliving_menu_requires_real_power_threshold_not_just_sibling_printed(self):
        for v in ('tapped_1','fresh_0'):
            self.assertFalse(any(s['op']=='choose_activation_mode' for s in self.case('Unliving Psychopath',v)['steps']))
        for owner in (0,1):
            c=self.case('Unliving Psychopath',f'lethal_repeat_{owner}')
            self.assertEqual(sum(s['op']=='choose_activation_mode' for s in c['steps']),3)
            self.assertEqual(sum(s['op']=='activate' and s['ref']=='source' for s in c['steps']),4)
            self.assertEqual(self.last(c,'assert_mana_total',player=owner),76)
            self.assertEqual(self.last(c,'assert_zone_multiplicity',player=owner,zone='graveyard',index=8309),1)

    def test_unliving_actual_zero_power_menu_is_input_only_not_oracle_target_legality(self):
        c=self.case('Unliving Psychopath','value_0');s=c['steps']
        self.assertEqual(sum(x['op']=='choose_activation_mode' for x in s),1)
        self.assertIn(dict(op='choose_activation_mode',ref='source',player=0,mode_label='Pump Unliving Psychopath',fallback_color=0,allow_auto_nonmana=False),s)
        self.assertFalse(any(x['op']=='choose_target' for x in s))
        self.assertEqual(self.last(c,'require_card',ref='source',property='power'),1)
        self.assertEqual(self.last(c,'require_card',ref='source',property='toughness'),3)
        self.assertEqual(self.last(c,'assert_mana_total',player=0),79)
        # One real counter makes power1: every fixture creature has power>=1,
        # so neither the legacy zero-power presentation nor real <power applies.
        c=self.case('Unliving Psychopath','counter_cleanup_0')
        self.assertFalse(any(x['op']=='choose_activation_mode' for x in c['steps']))
        self.assertEqual(self.last(c,'assert_counter',ref='source',counter_kind='plus1'),1)

    def test_psychatog_measured_ai_discard_is_an_actual_zone_choice_without_human_row_churn(self):
        human=self.case('Psychatog','value_0');ai=self.case('Psychatog','value_1')
        self.assertFalse(human['_recipe']['params']['interface']['discard_choices']['1'])
        self.assertTrue(ai['_recipe']['params']['interface']['discard_choices']['1'])
        choice=dict(op='choose_zone_card',player=1,value=self.rows['Plains']['csvid'])
        self.assertIn(choice,ai['steps']);self.assertEqual(self.last(ai,'require_card',ref='source',property='power'),2)
        d=self.declaration('Psychatog','value_1');d['variants']+=['value_0']
        with self.assertRaises(ValueError):model.spec(d,self.oracle['Psychatog'])

    def test_selected_cost_denial_keeps_only_real_affordable_sibling_global_permission(self):
        for nm,v,a in [('Narcissism','empty_hand_0',0),('Rakdos Guildmage','empty_hand_1',1),('Disciple of Tevesh Szat','insufficient_mana_1',1),('Disciple of Tevesh Szat','wrong_color_0',1)]:
            c=self.case(nm,v,a);self.assertEqual(self.last(c,'require_can_activate',ref='source'),1)
            self.assertFalse(any(s['op']=='activate' and s['ref']=='source' for s in c['steps']))

    def test_rusalka_observed_human_sacrifice_mark_precedes_effect_target_only_in_affected_cells(self):
        for v,targets in [('value_0',['victim0','ally']),('target_enemy_0',['victim0','enemy']),('sacrifice_source_0',['source','enemy'])]:
            c=self.case('Plagued Rusalka',v);s=c['steps'];i=next(i for i,x in enumerate(s) if x['op']=='activate' and x['ref']=='source')
            self.assertEqual([x['ref'] for x in s[:i] if x['op']=='choose_target'][-2:],targets)
        ai=self.case('Plagued Rusalka','value_1');self.assertFalse(ai['_recipe']['params']['interface']['sacrifice_before_target'])
        deny=self.case('Plagued Rusalka','insufficient_mana_0');self.assertFalse(deny['_recipe']['params']['interface']['sacrifice_before_target'])
        self.assertEqual(self.last(deny,'require_can_activate',ref='source'),0)

    def test_hybrid_cast_uses_declared_legal_colors_without_changing_activation_cost(self):
        for nm,color in [('Gruul Guildmage',3),('Rakdos Guildmage',1)]:
            c=self.case(nm);cast=next(i for i,s in enumerate(c['steps']) if s['op']=='cast' and s['ref']=='source')
            paid=[s for s in c['steps'][:cast] if s['op']=='set_mana'][-6:]
            self.assertEqual([s['value'] for s in paid],[2 if i==color else 0 for i in range(6)])

    def test_discard_negative_toughness_lethal_loop_provisions_each_real_cost(self):
        c=self.case('Ravenous Bloodseeker','lethal_repeat_1')
        self.assertEqual(sum(x['name']=='Plains' and x['player']==1 and x['zone']=='hand' for x in c['setup']),2)
        self.assertEqual(self.last(c,'assert_zone_multiplicity',player=1,zone='graveyard',index=self.rows['Plains']['csvid']),2)
        self.assertEqual(self.last(c,'assert_zone_multiplicity',player=1,zone='graveyard',index=15929),1)

    def test_modal_rules_cannot_be_transferred_to_unrelated_oracle(self):
        for nm,rule in [('Watercourser','grave_two_or_ai'),('Watercourser','power_less')]:
            d=self.declaration(nm);d['params']['interface']['menu_rule']=rule
            with self.assertRaises(ValueError):model.spec(d,self.oracle[nm])


if __name__=='__main__':unittest.main()
