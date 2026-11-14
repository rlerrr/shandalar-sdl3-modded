"""Independent fixed-counter, real-entry and mandatory callback receipts."""
import copy
import json
from pathlib import Path
import unittest

from generate_cases import registry
import permanent_plus1_activation_recipes as model


class PermanentPlus1ActivationRecipesTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows={r['name']:r for r in registry()}

    def declaration(self,nm,variant='value_0',ability=None):
        pin=model.pins()['sources'][nm]
        if ability is None:ability=int(next(iter(pin['abilities'])))
        return dict(family=model.FAMILY,version=2,oracle_text=self.oracle[nm]['oracle_text'],
                    params=dict(ability=ability,interface=copy.deepcopy(pin['abilities'][str(ability)]),
                    other_clauses=[line for i,line in enumerate(self.oracle[nm]['oracle_text'].splitlines()) if i!=ability]),variants=[variant])

    def case(self,nm,variant='value_0',ability=None):
        d=self.declaration(nm,variant,ability);p=model.spec(d,self.oracle[nm])
        return model.expand(nm,self.rows[nm]['csvid'],d,p,[variant],self.rows,self.oracle)[0]

    def last(self,c,op,**fields):
        return [s for s in c['steps'] if s['op']==op and all(s.get(k)==v for k,v in fields.items())][-1]['value']

    def pending(self,c):
        s=c['steps'];a=next(i for i,x in enumerate(s) if x['op']=='activate' and x['ref']=='source')
        b=next(i for i in range(a+1,len(s)) if s[i]['op']=='resolve')
        return s[a:b]

    def test_thirteen_distinct_implemented_normal_primaries_fourteen_selected_profiles(self):
        sources=model.pins()['sources'];self.assertEqual(len(sources),13);self.assertEqual(sum(len(s['abilities']) for s in sources.values()),14)
        ids=[]
        for nm,s in sources.items():
            self.assertEqual(self.rows[nm]['kind'],'card');self.assertNotIn('dummy',self.rows[nm]['handler'])
            self.assertEqual(s['oracle']['layout'],'normal');self.assertEqual(self.rows[nm]['csvid'],s['csvid']);ids.append(s['csvid'])
        self.assertEqual(len(ids),len(set(ids)))

    def test_printed_zero_bases_survive_only_actual_cast_entry_counters(self):
        for nm,initial in [('Arcbound Ravager',1),('Mikaeus, the Lunarch',2),('Hangarback Walker',2)]:
            c=self.case(nm);s=c['steps'];cast=next(i for i,x in enumerate(s) if x['op']=='cast' and x['ref']=='source')
            activate=next(i for i,x in enumerate(s) if x['op']=='activate' and x['ref']=='source')
            self.assertEqual(self.oracle[nm]['power'],'0');self.assertEqual(self.oracle[nm]['toughness'],'0')
            self.assertIn(dict(op='assert_counter',ref='source',counter_kind='plus1',value=initial),s[cast:activate])
            self.assertFalse(any(x['op'] in ('set_counter','add_counter','set_power','set_toughness') for x in s))

    def test_x_and_double_x_casts_pay_real_distinct_printed_costs(self):
        for nm,cost in [('Mikaeus, the Lunarch',[2,0,0,0,0,1]),('Hangarback Walker',[4,0,0,0,0,0])]:
            c=self.case(nm);s=c['steps'];cast=next(i for i,x in enumerate(s) if x['op']=='cast')
            self.assertEqual([x['value'] for x in s[:cast] if x['op']=='set_mana'],cost)
            self.assertIn(dict(op='choose_x',player=0,value=2),s[:cast])
            self.assertIn(dict(op='assert_mana_total',player=0,value=0),s[cast:])

    def test_zero_x_has_real_zero_zero_death_and_no_activation_or_token(self):
        for nm in ('Mikaeus, the Lunarch','Hangarback Walker'):
            c=self.case(nm,'entry_x0_1')
            self.assertIn(dict(op='choose_x',player=1,value=0),c['steps'])
            self.assertFalse(any(s['op']=='activate' for s in c['steps']))
            self.assertEqual(self.last(c,'assert_zone_multiplicity',player=1,zone='graveyard',index=self.rows[nm]['csvid']),1)
            self.assertEqual(self.last(c,'assert_zone_multiplicity',player=1,zone='battlefield',index=self.rows['Thopter']['csvid']),0)

    def test_converge_uses_actual_paid_colors_and_printed_one_one_base(self):
        for owner in (0,1):
            c=self.case('Crystalline Crawler',f'value_{owner}');s=c['steps'];cast=next(i for i,x in enumerate(s) if x['op']=='cast')
            self.assertEqual([x['value'] for x in s[:cast] if x['op']=='set_mana'],[0,2,2,2,2,0])
            self.assertIn(dict(op='assert_counter',ref='source',counter_kind='plus1',value=4),s[:next(i for i,x in enumerate(s) if x['op']=='activate')])
            self.assertEqual(self.last(c,'assert_counter',ref='source',counter_kind='plus1'),5)
            self.assertEqual(self.last(c,'require_card',ref='source',property='power'),6)
            self.assertEqual(self.last(c,'require_card',ref='source',property='toughness'),6)
            self.assertEqual([x['color'] for x in s[:cast] if x['op']=='choose_mana'],[1,2,3,4]*2 if owner==0 else [])

    def test_converge_zero_and_one_color_are_actual_payment_variants(self):
        for mode,counters,power in [('converge_zero',1,2),('converge_one',2,3)]:
            c=self.case('Crystalline Crawler',mode+'_0')
            self.assertEqual(self.last(c,'assert_counter',ref='source',counter_kind='plus1'),counters)
            self.assertEqual(self.last(c,'require_card',ref='source',property='power'),power)

    def test_hybrid_cast_is_real_legal_GG_and_ability_payment_is_four_G(self):
        c=self.case('Golgari Guildmage','payment_0');s=c['steps'];cast=next(i for i,x in enumerate(s) if x['op']=='cast')
        self.assertEqual([x['value'] for x in s[:cast] if x['op']=='set_mana'],[0,0,0,2,0,0])
        self.assertEqual(self.last(c,'assert_mana_total',player=0),0)
        self.assertEqual(self.last(c,'require_card',ref='ally',property='power'),2)

    def test_mikaeus_counter_cost_changes_own_stats_before_other_creatures_grow(self):
        c=self.case('Mikaeus, the Lunarch','pending_snapshot_1',2);pending=self.pending(c)
        self.assertIn(dict(op='assert_counter',ref='source',counter_kind='plus1',value=1),pending)
        self.assertIn(dict(op='require_card',ref='source',property='power',value=1),pending)
        self.assertIn(dict(op='assert_counter',ref='ally',counter_kind='plus1',value=0),pending)
        self.assertEqual(self.last(c,'assert_counter',ref='source',counter_kind='plus1'),1)
        self.assertEqual(self.last(c,'assert_counter',ref='ally',counter_kind='plus1'),1)
        self.assertEqual(self.last(c,'assert_counter',ref='ally_artifact',counter_kind='plus1'),1)
        self.assertEqual(self.last(c,'assert_counter',ref='enemy',counter_kind='plus1'),0)

    def test_last_mikaeus_counter_can_be_paid_even_though_source_dies_before_effect(self):
        c=self.case('Mikaeus, the Lunarch','last_counter_0',2)
        self.assertEqual(self.last(c,'assert_zone_multiplicity',player=0,zone='battlefield',index=self.rows['Mikaeus, the Lunarch']['csvid']),0)
        self.assertEqual(self.last(c,'assert_counter',ref='ally',counter_kind='plus1'),1)
        self.assertEqual(self.last(c,'assert_counter',ref='ally_artifact',counter_kind='plus1'),1)

    def test_fresh_tap_cost_denial_is_not_a_false_global_mana_denial(self):
        human=self.case('Crystalline Crawler','fresh_0');ai=self.case('Crystalline Crawler','fresh_1')
        self.assertIn(dict(op='require_can_activate',ref='source',value=1),human['steps'])
        self.assertFalse(any(s['op']=='activate' for s in human['steps']))
        self.assertIn(dict(op='choose_dialog',player=1,value=1),ai['steps'])
        self.assertIn(dict(op='activate',ref='source',value=0),ai['steps'])
        self.assertIn(dict(op='require_can_activate',ref='source',value=0),self.case('Hangarback Walker','fresh_0')['steps'])

    def test_repeat_crawler_retains_legal_counter_funded_mana_sibling(self):
        for owner in (0,1):
            c=self.case('Crystalline Crawler',f'repeat_{owner}')
            self.assertEqual(self.last(c,'require_can_activate',ref='source'),1)
            self.assertEqual(self.last(c,'assert_counter',ref='source',counter_kind='plus1'),5)

    def test_actual_untap_then_second_remove_counter_can_kill_mikaeus(self):
        c=self.case('Mikaeus, the Lunarch','repeat_untap_0',2)
        self.assertIn(dict(op='cast',ref='untap',value=1),c['steps'])
        self.assertEqual(self.last(c,'assert_counter',ref='ally',counter_kind='plus1'),2)
        self.assertEqual(self.last(c,'assert_zone_multiplicity',player=0,zone='graveyard',index=self.rows['Mikaeus, the Lunarch']['csvid']),1)

    def test_steadfast_pays_minus_two_and_affects_only_own_creatures_other_walkers(self):
        c=self.case('Ajani Steadfast','pending_snapshot_0')
        self.assertEqual(self.last(c,'assert_counter',ref='source',counter_kind='loyalty'),2)
        self.assertEqual(self.last(c,'assert_counter',ref='ally_walker',counter_kind='loyalty'),4)
        self.assertEqual(self.last(c,'assert_counter',ref='enemy_walker',counter_kind='loyalty'),3)
        self.assertEqual(self.last(c,'assert_counter',ref='ally',counter_kind='plus1'),1)
        self.assertEqual(self.last(c,'assert_counter',ref='enemy',counter_kind='plus1'),0)
        self.assertEqual(sum(s['op']=='cast' for s in c['steps']),3)
        self.assertNotEqual(next(x['name'] for x in c['setup'] if x['ref']=='ally_walker'),next(x['name'] for x in c['setup'] if x['ref']=='enemy_walker'))

    def test_planeswalker_offturn_and_second_activation_remain_denied(self):
        for nm in ('Ajani Steadfast','Ajani, Adversary of Tyrants'):
            self.assertIn(dict(op='activate',ref='source',value=0),self.case(nm,'offturn_1')['steps'])
            self.assertEqual([s['value'] for s in self.case(nm,'repeat_1')['steps'] if s['op']=='activate'],[1,0])

    def test_adversary_zero_one_two_actual_distinct_targets_with_no_cost_refund(self):
        for mode,refs in [('targets0',[]),('targets1',['ally']),('value',['ally','enemy'])]:
            c=self.case('Ajani, Adversary of Tyrants',mode+'_0')
            self.assertEqual([s['ref'] for s in c['steps'] if s['op']=='choose_target'],refs)
            self.assertEqual(sum(s['op']=='choose_cancel_target' for s in c['steps']),int(len(refs)<2))
            self.assertEqual(self.last(c,'assert_counter',ref='source',counter_kind='loyalty'),5)
            self.assertEqual(self.last(c,'assert_counter',ref='ally',counter_kind='plus1'),int('ally' in refs))
            self.assertEqual(self.last(c,'assert_counter',ref='enemy',counter_kind='plus1'),int('enemy' in refs))

    def test_one_removed_optional_target_does_not_prevent_other_valid_target(self):
        c=self.case('Ajani, Adversary of Tyrants','target_removed_pending_1')
        self.assertIn(dict(op='destroy',ref='ally'),c['steps'])
        self.assertEqual(self.last(c,'assert_counter',ref='enemy',counter_kind='plus1'),1)
        self.assertEqual(self.last(c,'assert_counter',ref='source',counter_kind='loyalty'),5)

    def test_simic_mandatory_growth_is_zero_pending_and_one_after_target_counter(self):
        for owner in (0,1):
            c=self.case('Simic Ascendancy',f'pending_snapshot_{owner}')
            self.assertIn(dict(op='assert_counter',ref='source',counter_kind='growth',value=0),self.pending(c))
            self.assertIn(dict(op='assert_counter',ref='ally',counter_kind='plus1',value=0),self.pending(c))
            self.assertEqual(self.last(c,'assert_counter',ref='source',counter_kind='growth'),1)
            self.assertEqual(self.last(c,'assert_counter',ref='ally',counter_kind='plus1'),1)
            self.assertEqual(self.last(c,'require_card',ref='ally',property='toughness'),3)

    def test_simic_enemy_target_is_probed_illegal_and_spends_nothing(self):
        c=self.case('Simic Ascendancy','target_enemy_0')
        self.assertIn(dict(op='probe_activation_target',ref='source',other='enemy',index=-1,value=0),c['steps'])
        self.assertFalse(any(s['op']=='activate' for s in c['steps']))
        self.assertEqual(self.last(c,'assert_counter',ref='source',counter_kind='growth'),0)
        self.assertEqual(self.last(c,'assert_mana_total',player=0),80)

    def test_simic_removed_target_has_no_counter_or_growth_trigger(self):
        c=self.case('Simic Ascendancy','target_removed_pending_0')
        self.assertEqual(self.last(c,'assert_counter',ref='source',counter_kind='growth'),0)
        self.assertEqual(self.last(c,'assert_counter',ref='enemy',counter_kind='plus1'),0)

    def test_ravager_cost_is_real_first_MV_zero_artifact_with_enemy_resources_untouched(self):
        for owner in (0,1):
            c=self.case('Arcbound Ravager',f'value_{owner}')
            self.assertEqual(c['setup'][0]['name'],'Ornithopter');self.assertEqual(c['setup'][0]['player'],owner)
            self.assertEqual([s['ref'] for s in c['steps'] if s['op']=='choose_target'],['victim'] if owner==0 else [])
            self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='graveyard',index=self.rows['Ornithopter']['csvid'],value=1),self.pending(c))
            self.assertEqual(self.last(c,'assert_counter',ref='source',counter_kind='plus1'),2)
            self.assertEqual(self.last(c,'assert_counter',ref='enemy_artifact',counter_kind='plus1'),0)

    def test_modular_death_after_growth_transfers_actual_two_counters(self):
        c=self.case('Arcbound Ravager','source_dies_after_1')
        self.assertIn(dict(op='choose_target',player=1,ref='ally_artifact'),c['steps'])
        self.assertEqual(self.last(c,'assert_counter',ref='ally_artifact',counter_kind='plus1'),2)
        self.assertEqual(self.last(c,'require_card',ref='ally_artifact',property='power'),3)
        self.assertEqual(self.last(c,'assert_zone_multiplicity',player=1,zone='battlefield',index=self.rows['Arcbound Ravager']['csvid']),0)

    def test_modular_decline_is_actual_target_cancel_not_missing_death_effect(self):
        c=self.case('Arcbound Ravager','death_decline_0')
        self.assertIn(dict(op='choose_cancel_target',player=0),c['steps'])
        self.assertEqual(self.last(c,'assert_counter',ref='ally_artifact',counter_kind='plus1'),0)

    def test_hangarback_pending_death_uses_two_counters_and_postresolution_death_three(self):
        for mode,count in [('source_removed_pending',2),('source_dies_after',3)]:
            c=self.case('Hangarback Walker',mode+'_0')
            self.assertEqual(self.last(c,'assert_zone_multiplicity',player=0,zone='battlefield',index=self.rows['Thopter']['csvid']),count)
            for prop,value in [('color',0),('artifact',1),('subtype_Thopter',1),('power',1),('toughness',1),('token',1)]:
                self.assertEqual(self.last(c,'assert_matching_permanents',player=0,index=self.rows['Thopter']['csvid'],property=prop),value)
            self.assertEqual(self.oracle['Thopter']['colors'],['U'])

    def test_hangarback_flying_is_exercised_by_actual_blocking(self):
        c=self.case('Hangarback Walker','token_block_flying_1')
        self.assertIn(dict(op='block',ref='combat_token',other='flier',value=1),c['steps'])
        self.assertEqual(self.last(c,'assert_zone_multiplicity',player=1,zone='battlefield',index=self.rows['Thopter']['csvid']),2)
        self.assertEqual(self.last(c,'assert_zone_multiplicity',player=0,zone='graveyard',index=self.rows['Suntail Hawk']['csvid']),1)

    def test_flying_witness_is_present_before_earlier_pending_receipts(self):
        c=self.case('Hangarback Walker','token_block_flying_0')
        self.assertIn(dict(op='require_card',ref='flier',property='in_play',value=1),self.pending(c))

    def test_death_snapshot_replays_real_callback_and_snapshot_restores_tokens(self):
        for nm in ('Arcbound Ravager','Hangarback Walker'):
            c=self.case(nm,'death_snapshot_1')
            for op in ('capture','restore','ai_roundtrip','assert_recorded_state'):self.assertEqual(sum(s['op']==op for s in c['steps']),1)
            self.assertEqual(sum(s['op']=='destroy' and s['ref']=='source' for s in c['steps']),2)

    def test_pending_snapshot_preserves_cost_counter_and_target_growth(self):
        c=self.case('Mikaeus, the Lunarch','pending_snapshot_0',2)
        self.assertEqual(sum(s['op']=='activate' for s in c['steps']),1)
        self.assertEqual(sum(s['op']=='restore' for s in c['steps']),1)
        self.assertEqual(self.last(c,'assert_counter',ref='ally',counter_kind='plus1'),1)

    def test_source_removed_from_pending_golgari_does_not_cancel_target_effect(self):
        c=self.case('Golgari Guildmage','source_removed_pending_0')
        self.assertEqual(self.last(c,'assert_counter',ref='ally',counter_kind='plus1'),1)
        self.assertEqual(self.last(c,'assert_zone_multiplicity',player=0,zone='battlefield',index=self.rows['Golgari Guildmage']['csvid']),0)

    def test_dead_source_receipts_never_query_a_recycled_source_slot(self):
        c=self.case('Hangarback Walker','source_dies_after_0');s=c['steps'];dead=next(i for i,x in enumerate(s) if x['op']=='destroy')
        self.assertFalse(any(x.get('ref')=='source' and x['op'] in ('require_card','assert_counter') for x in s[dead:]))

    def test_no_fake_upkeep_win_or_counter_stat_setters(self):
        for nm in model.pins()['sources']:
            c=self.case(nm)
            self.assertFalse(any(s['op'] in ('upkeep','first_main','set_counter','set_power','set_toughness') for s in c['steps']))
            self.assertTrue(all('state' in x for x in c['setup'] if x['zone']=='battlefield'))

    def test_contract_family_version_variants_and_complete_source_are_fail_closed(self):
        nm='Simic Ascendancy'
        for key,value in [('family','charge_counter_activation'),('version',True),('version',1),('variants',[]),('variants',['value_0','value_0']),('variants',['bogus_0']),('variants',[1]),('oracle_text','Put a +1/+1 counter on target creature.')]:
            d=self.declaration(nm);d[key]=value
            with self.subTest(key=key,value=value),self.assertRaises(ValueError):model.spec(d,self.oracle[nm])

    def test_whole_source_interface_and_inactive_clauses_are_independent_pins(self):
        d=self.declaration('Crystalline Crawler');d['params']['interface']['converge_colors']=['G']
        with self.assertRaises(ValueError):model.spec(d,self.oracle['Crystalline Crawler'])
        d=self.declaration('Hangarback Walker');d['params']['other_clauses']=[]
        with self.assertRaises(ValueError):model.spec(d,self.oracle['Hangarback Walker'])

    def test_witness_mutation_is_rejected(self):
        changed=copy.deepcopy(self.oracle);changed['Coretapper']['toughness']='2'
        with self.assertRaises(ValueError):model.validate_witnesses(changed,self.rows)

    def test_entry_death_and_growth_clauses_are_composed_not_listed_as_untested(self):
        for nm,clause in [('Arcbound Ravager','Modular 1'),('Hangarback Walker','When this creature dies, create'),
                           ('Simic Ascendancy','Whenever one or more +1/+1 counters')]:
            c=self.case(nm)
            self.assertTrue(any(clause in line for line in c['_recipe']['params']['other_clauses']))
            self.assertFalse(any(clause in line for line in c['_recipe']['untested_components']))

    def test_double_x_probe_measures_overcharge_without_changing_entry_or_printed_payment(self):
        home=Path(__file__).parent/'data/cases/cards/151/15144-hangarback-walker.json'
        c=json.loads(home.read_text(encoding='utf8'))['cases'][0]
        self.assertEqual(c['name'],'PermanentPlus1Activation_HangarbackDoubleXCostProbe_0')
        self.assertEqual(c['setup'],[dict(name='Hangarback Walker',ref='source',player=0,zone='hand')])
        s=c['steps'];cast=next(i for i,x in enumerate(s) if x['op']=='cast')
        self.assertEqual([x['value'] for x in s[:cast] if x['op']=='set_mana'],[6,0,0,0,0,0])
        self.assertEqual([x['value'] for x in s[:cast] if x['op']=='choose_x'],[2,2])
        self.assertEqual(self.oracle['Hangarback Walker']['mana_cost'],'{X}{X}')
        self.assertTrue(all(x['value']==2 for x in s if x['op']=='assert_mana_total'))
        self.assertIn(dict(op='assert_counter',ref='source',counter_kind='plus1',value=2),s)
        self.assertTrue(all(x['value']==2 for x in s if x['op']=='assert_card' and x['property'] in ('power','toughness')))
        self.assertFalse(any(x['op'] in ('set_counter','add_counter','set_power','set_toughness') for x in s))

    def test_legacy_placeholder_is_scoped_input_and_preserves_declared_x_cost_and_entry(self):
        nm='Hangarback Walker';c=self.case(nm,'source_removed_pending_0');s=c['steps']
        cast=next(i for i,x in enumerate(s) if x['op']=='cast')
        self.assertEqual([x['value'] for x in s[:cast] if x['op']=='choose_x'],[0,2])
        self.assertEqual([x['value'] for x in s[:cast] if x['op']=='set_mana'],[4,0,0,0,0,0])
        self.assertIn(dict(op='assert_mana_total',player=0,value=0),s[cast:])
        self.assertIn(dict(op='assert_counter',ref='source',counter_kind='plus1',value=2),s[cast:])
        self.assertNotIn('casting_protocols',c['_recipe']['params'])
        self.assertNotIn('casting_protocols',c['_recipe']['params']['interface'])
        plain=self.case(nm);a=next(i for i,x in enumerate(plain['steps']) if x['op']=='cast')
        self.assertEqual([x['value'] for x in plain['steps'][:a] if x['op']=='choose_x'],[0,2])

    def test_owner1_hangarback_retains_original_sole_declared_x_input(self):
        c=self.case('Hangarback Walker','source_removed_pending_1');s=c['steps']
        cast=next(i for i,x in enumerate(s) if x['op']=='cast')
        self.assertEqual([x['value'] for x in s[:cast] if x['op']=='choose_x'],[2])
        self.assertEqual([x['player'] for x in s[:cast] if x['op']=='choose_x'],[1])
        self.assertEqual([x['value'] for x in s[:cast] if x['op']=='set_mana'],[4,0,0,0,0,0])
        self.assertIn(dict(op='assert_counter',ref='source',counter_kind='plus1',value=2),s)

    def test_crawler_probe_reuses_declared_colors_and_retains_printed_payment_entry_and_base(self):
        home=Path(__file__).parent/'data/cases/cards/165/16597-crystalline-crawler.json'
        c=json.loads(home.read_text(encoding='utf8'))['cases'][0];s=c['steps']
        cast=next(i for i,x in enumerate(s) if x['op']=='cast')
        self.assertEqual(c['name'],'PermanentPlus1Activation_CrawlerConvergeCostProbe_0')
        self.assertEqual([x['value'] for x in s[:cast] if x['op']=='set_mana'],[0,2,2,2,2,0])
        self.assertEqual([x['color'] for x in s[:cast] if x['op']=='choose_mana'],[1,2,3,4]*2)
        self.assertEqual(self.oracle['Crystalline Crawler']['mana_cost'],'{4}')
        self.assertEqual({x['color']:x['value'] for x in s if x['op']=='assert_mana'},{0:0,1:1,2:1,3:1,4:1,5:0})
        self.assertTrue(all(x['value']==4 for x in s if x['op']=='assert_mana_total'))
        self.assertIn(dict(op='assert_counter',ref='source',counter_kind='plus1',value=4),s)
        self.assertTrue(all(x['value']==5 for x in s if x['op']=='assert_card'))

    def test_functional_crawler_excess_funding_keeps_correct_single_payment_receipts(self):
        for owner in (0,1):
            for mode,vector in [('pending_snapshot',[0,1,1,1,1,0]),('converge_zero',[4,0,0,0,0,0]),('converge_one',[0,0,0,4,0,0])]:
                c=self.case('Crystalline Crawler',f'{mode}_{owner}');s=c['steps'];cast=next(i for i,x in enumerate(s) if x['op']=='cast')
                self.assertEqual([x['value'] for x in s[:cast] if x['op']=='set_mana'],[v*2 for v in vector])
                first_total=next(x['value'] for x in s[cast:] if x['op']=='assert_mana_total')
                self.assertEqual(first_total,4)
                received={x['color']:x['value'] for x in s[cast:] if x['op']=='assert_mana' and x['player']==owner}
                self.assertEqual(received,dict(enumerate(vector)))

    def test_all_functional_hangarback_inputs_preserve_actual_declared_x_and_twice_x_cost(self):
        nm='Hangarback Walker';p=model.parse(self.oracle[nm],2)
        self.assertEqual(len(model.variants(p)),38)
        for variant in model.variants(p):
            mode,who=variant.rsplit('_',1);owner=int(who)
            x=int(mode[-1]) if mode in ('entry_x0','entry_x1','entry_x3') else 2
            c=self.case(nm,variant);s=c['steps'];cast=next(i for i,a in enumerate(s) if a['op']=='cast')
            self.assertEqual([a['value'] for a in s[:cast] if a['op']=='choose_x'],[0,x] if owner==0 else [x])
            self.assertEqual([a['value'] for a in s[:cast] if a['op']=='set_mana'],[2*x,0,0,0,0,0])
            self.assertEqual(next(a['value'] for a in s[cast:] if a['op']=='assert_mana_total'),0)
            if x:self.assertIn(dict(op='assert_counter',ref='source',counter_kind='plus1',value=x),s)

    def test_last_counter_cost_recalculates_sbas_before_pending_death_receipts(self):
        for owner in (0,1):
            for mode in ('entry_x1','last_counter','repeat_untap'):
                c=self.case('Mikaeus, the Lunarch',f'{mode}_{owner}',2);s=c['steps']
                deaths=[i for i,a in enumerate(s) if a['op']=='assert_zone_multiplicity' and a.get('zone')=='battlefield' and a.get('index')==12022 and a['value']==0]
                self.assertTrue(deaths)
                i=max(j for j,a in enumerate(s[:deaths[0]]) if a['op']=='activate')
                self.assertEqual(s[i+1],dict(op='refresh'))
                self.assertFalse(any(a['op']=='resolve' for a in s[i+1:deaths[0]]))
        c=self.case('Mikaeus, the Lunarch','value_0',2);s=c['steps'];i=next(i for i,a in enumerate(s) if a['op']=='activate')
        self.assertEqual(s[i+1],dict(op='assert_stack',value=1))

    def test_caller_zero_target_pays_loyalty_without_counter_and_accepts_enemy_target(self):
        c=self.case('Ajani, Caller of the Pride','targets0_0');s=c['steps']
        self.assertEqual(self.last(c,'assert_counter',ref='source',counter_kind='loyalty'),5)
        self.assertEqual(self.last(c,'assert_counter',ref='ally',counter_kind='plus1'),0)
        self.assertIn(dict(op='choose_optional_target',player=0,ref='ally'),s)
        self.assertIn(dict(op='assert_forced_target_choices',value=0),s)
        enemy=self.case('Ajani, Caller of the Pride','target_enemy_1')
        self.assertEqual(self.last(enemy,'assert_counter',ref='enemy',counter_kind='plus1'),1)

    def test_gavony_is_a_land_fixture_and_pays_exact_two_green_white_for_own_creatures(self):
        c=self.case('Gavony Township','payment_0');s=c['steps']
        self.assertEqual(next(x for x in c['setup'] if x['ref']=='source')['zone'],'battlefield')
        self.assertFalse(any(x['op']=='cast' and x['ref']=='source' for x in s))
        a=next(i for i,x in enumerate(s) if x['op']=='activate' and x['ref']=='source')
        funded=[x for x in s[:a] if x['op']=='set_mana' and x['player']==0][-6:]
        self.assertEqual([x['value'] for x in funded],[2,0,0,1,0,1])
        self.assertEqual(s[a+1],dict(op='require_activation_mode',value=1))
        self.assertEqual(self.last(c,'assert_counter',ref='ally',counter_kind='plus1'),1)
        self.assertEqual(self.last(c,'assert_counter',ref='source',counter_kind='plus1'),0)
        self.assertEqual(self.last(c,'assert_counter',ref='enemy',counter_kind='plus1'),0)
        self.assertEqual(self.last(c,'assert_counter',ref='ally_walker',counter_kind='loyalty'),3)

    def test_gavony_sibling_permission_remains_true_without_selected_colors_and_false_tapped(self):
        for owner in (0,1):
            c=self.case('Gavony Township',f'wrong_color_{owner}')
            self.assertEqual(self.last(c,'require_can_activate',ref='source'),1)
            self.assertFalse(any(x['op']=='activate' and x['ref']=='source' for x in c['steps']))
            t=self.case('Gavony Township',f'tapped_{owner}')
            self.assertEqual(self.last(t,'require_can_activate',ref='source'),0)

    def test_ballista_actor_specific_x_inputs_are_independent_and_probe_retains_double_declared_x(self):
        for owner in (0,1):
            c=self.case('Walking Ballista',f'value_{owner}');s=c['steps'];a=next(i for i,x in enumerate(s) if x['op']=='cast')
            self.assertEqual([x['value'] for x in s[:a] if x['op']=='choose_x'],[0,2] if owner==0 else [2])
            self.assertEqual([x['value'] for x in s[:a] if x['op']=='set_mana'],[4,0,0,0,0,0])
            self.assertEqual(self.last(c,'assert_counter',ref='source',counter_kind='plus1'),3)
        home=Path(__file__).parent/'data/cases/cards/168/16822-walking-ballista.json'
        c=json.loads(home.read_text(encoding='utf8'))['cases'][0];s=c['steps']
        self.assertEqual([x['value'] for x in s if x['op']=='choose_x'],[2,2])
        self.assertEqual([x['value'] for x in s if x['op']=='set_mana'],[6,0,0,0,0,0])
        self.assertTrue(all(x['value']==2 for x in s if x['op']=='assert_mana_total'))

    def test_ballista_shortage_keeps_legal_counter_funded_ping_without_taking_it(self):
        human=self.case('Walking Ballista','insufficient_mana_0');ai=self.case('Walking Ballista','insufficient_mana_1')
        self.assertEqual(self.last(human,'require_can_activate',ref='source'),1)
        self.assertFalse(any(x['op']=='activate' and x['ref']=='source' for x in human['steps']))
        self.assertIn(dict(op='choose_dialog',player=1,value=0),ai['steps'])
        self.assertIn(dict(op='activate',ref='source',value=0),ai['steps'])
        for c in (human,ai):self.assertEqual(self.last(c,'assert_counter',ref='source',counter_kind='plus1'),2)

    def test_razerunners_real_land_cost_precedes_self_counter_and_endstep_stays_held(self):
        for owner in (0,1):
            c=self.case('Goblin Razerunners',f'payment_{owner}');pending=self.pending(c)
            self.assertEqual(next(x for x in c['setup'] if x['ref']=='victim')['name'],'Plains')
            self.assertIn(dict(op='assert_zone_multiplicity',player=owner,zone='graveyard',index=self.rows['Plains']['csvid'],value=1),pending)
            self.assertIn(dict(op='assert_counter',ref='source',counter_kind='plus1',value=0),pending)
            self.assertEqual(self.last(c,'require_card',ref='source',property='power'),4)
            self.assertEqual(self.last(c,'require_card',ref='source',property='toughness'),5)
            self.assertFalse(any(x['op'] in ('cleanup','advance_turn') for x in c['steps']))
            self.assertIn(dict(op='require_card',ref='source',property='sick',value=1),pending)
            p=model.parse(self.oracle['Goblin Razerunners'],0);self.assertNotIn(f'cleanup_{owner}',model.variants(p))
            no=self.case('Goblin Razerunners',f'no_land_{owner}')
            self.assertEqual(self.last(no,'require_can_activate',ref='source'),0)
            self.assertTrue(any(x['ref']=='enemy_land' and x['zone']=='battlefield' for x in no['setup']))

    def test_shalai_group_counts_itself_but_not_planeswalker_or_enemy(self):
        c=self.case('Shalai, Voice of Plenty','payment_1')
        for key in ('source','ally','ally_artifact'):self.assertEqual(self.last(c,'assert_counter',ref=key,counter_kind='plus1'),1)
        self.assertEqual(self.last(c,'assert_counter',ref='enemy',counter_kind='plus1'),0)
        self.assertEqual(self.last(c,'assert_counter',ref='ally_walker',counter_kind='loyalty'),3)
        self.assertEqual(self.last(c,'require_card',ref='source',property='power'),4)
        self.assertEqual(self.last(c,'require_card',ref='source',property='toughness'),5)

    def test_shalai_uses_actual_rod_target_predicates_and_player_only_spell_query(self):
        c=self.case('Shalai, Voice of Plenty','hexproof_flying_1');s=c['steps']
        for target in ('ally','ally_walker'):
            self.assertIn(dict(op='probe_activation_target',ref='enemy_rod',other=target,index=-1,value=0),s)
            self.assertIn(dict(op='probe_activation_target',ref='enemy_rod',other=target,index=-1,value=1),s)
        self.assertIn(dict(op='probe_activation_target',ref='enemy_rod',other='source',index=-1,value=1),s)
        self.assertIn(dict(op='probe_activation_target',ref='own_rod',other='ally_walker',index=-1,value=1),s)
        self.assertIn(dict(op='can_cast',ref='duress',value=0),s);self.assertIn(dict(op='can_cast',ref='duress',value=1),s)
        self.assertIn(dict(op='choose_player',player=1,value=1),s)
        self.assertIn(dict(op='cast',ref='self_spell',value=1),s)
        self.assertIn(dict(op='block',ref='source',other='flier',value=1),s)
        self.assertFalse(any(x in ('Flying','You, planeswalkers you control, and other creatures you control have hexproof.') for x in c['_recipe']['untested_components']))

    def test_shalai_flying_is_real_combat_with_marked_damage_and_unblocked_player_damage(self):
        c=self.case('Shalai, Voice of Plenty','flying_attack_0')
        self.assertIn(dict(op='block',ref='enemy_artifact',other='source',value=0),c['steps'])
        self.assertEqual(self.last(c,'assert_life',player=1),16)
        b=self.case('Shalai, Voice of Plenty','flying_block_1')
        self.assertEqual(self.last(b,'assert_card',ref='source',property='damage'),1)
        self.assertEqual(self.last(b,'assert_zone_multiplicity',player=0,zone='graveyard',index=self.rows['Suntail Hawk']['csvid']),1)


if __name__=='__main__':unittest.main()
