"""Independent count scopes, actual entry costs and irreversible-action receipts."""
import copy
import json
import unittest
from pathlib import Path
from unittest.mock import patch

from generate_cases import registry
import counted_tap_mana_activation_recipes as m


class CountedTapManaActivationRecipesTest(unittest.TestCase):
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
    def activate(self,c):return next(i for i,s in enumerate(c['steps']) if s['op']=='activate' and s['ref']=='source')
    def test_twelve_full_normal_implemented_sources_fifteen_profiles_and_independent_witnesses(self):
        data=m.pins();self.assertEqual(len(data['sources']),12);self.assertEqual(sum(len(s['abilities']) for s in data['sources'].values()),15)
        for n,s in {**data['sources'],**data['witnesses']}.items():
            self.assertEqual(s['oracle'],self.oracle[n]);self.assertEqual(s['oracle']['layout'],'normal');self.assertEqual(self.rows[n]['kind'],'card');self.assertEqual(s['handler'],self.rows[n]['handler']);self.assertEqual(s['csvid'],self.rows[n]['csvid']);self.assertNotIn('dummy',s['handler'])
    def test_whole_printed_clauses_and_native_entry_costs_fail_closed(self):
        for text in ('{T}: Add {C}. Draw a card.','Flying\n{T}: Add {C}.','{T}, Sacrifice this artifact: Add {C}.','{T}: Add X mana of any one color.'):
            r=copy.deepcopy(self.oracle['Cloudpost']);r['oracle_text']=text
            with self.assertRaises(ValueError):m.parse(r,len(text.splitlines())-1)
        r=copy.deepcopy(self.oracle['Astral Cornucopia']);r['mana_cost']='{X}{X}'
        with self.assertRaises(ValueError):m.parse(r,1)
        r=copy.deepcopy(self.oracle['Everflowing Chalice']);r['oracle_text']=r['oracle_text'].replace('Multikicker {2}','Multikicker {1}')
        with self.assertRaises(ValueError):m.parse(r,2)
    def test_family_version_identity_interface_and_variants_fail_closed(self):
        for k,v in [('family','printed_tap_mana_activation'),('version',True),('variants',[]),('variants',['payment_0','payment_0']),('variants',['other_1']),('variants',[1])]:
            d=self.declaration('Cloudpost');d[k]=v
            with self.assertRaises(ValueError):m.spec(d,self.oracle['Cloudpost'])
        d=self.declaration('Cloudpost');d['params']['ability']=True
        with self.assertRaises(ValueError):m.spec(d,self.oracle['Cloudpost'])
        d=self.declaration('Cloudpost');d['oracle_text']='{T}: Add {C}.'
        with self.assertRaises(ValueError):m.spec(d,self.oracle['Cloudpost'])
        d=self.declaration('Wirewood Channeler');d['params']['interface']['color_choice']='none'
        with self.assertRaises(ValueError):m.spec(d,self.oracle['Wirewood Channeler'])
    def test_unimplemented_primary_or_changed_witness_rejected(self):
        d=self.declaration('Cloudpost');p=m.spec(d,self.oracle['Cloudpost']);rows=copy.deepcopy(self.rows);rows['Cloudpost']['handler']='card_dummy'
        with self.assertRaises(ValueError):m.expand('Cloudpost',6637,d,p,['payment_0'],rows,self.oracle)
        rows=copy.deepcopy(self.rows);rows['Twiddle']['kind']='token'
        with self.assertRaises(ValueError):m.expand('Cloudpost',6637,d,p,['payment_0'],rows,self.oracle)
    def test_locus_counts_source_and_opponent_then_real_removal_changes_second_output(self):
        self.assertEqual(self.last(self.case('Cloudpost'),'assert_mana',player=0,color=0),3)
        self.assertEqual(self.last(self.case('Cloudpost','minimal_count_0'),'assert_mana',player=0,color=0),1)
        self.assertEqual(self.last(self.case('Cloudpost','wrong_owner_1'),'assert_mana',player=1,color=0),3)
        c=self.case('Cloudpost','dynamic_count_0');self.assertEqual(self.last(c,'assert_mana',player=0,color=0),5);self.assertIn(dict(op='destroy',ref='support_b'),c['steps']);self.assertIn(dict(op='cast',ref='untap',value=1),c['steps'])
    def test_urza_all_parts_are_owned_and_duplicates_do_not_replace_a_missing_part(self):
        for v,expected in [('payment_0',3),('partial_count_0',1),('minimal_count_0',1),('wrong_owner_0',1),('contrary_kind_0',1),('dynamic_count_0',4)]:self.assertEqual(self.last(self.case("Urza's Tower",v),'assert_mana',player=0,color=0),expected)
        self.assertEqual(m.mana_clause(self.oracle["Urza's Mine"]['oracle_text'])['enhanced'],2)
    def test_basic_swamp_count_excludes_nonbasic_swamp_and_enemy_basics(self):
        for who in (0,1):
            c=self.case('Cabal Stronghold',f'payment_{who}',1);self.assertEqual(self.last(c,'assert_mana',player=who,color=0),0);self.assertEqual(self.last(c,'assert_mana',player=who,color=1),3)
            for mode in ('minimal_count','wrong_owner','contrary_kind'):self.assertEqual(self.last(self.case('Cabal Stronghold',f'{mode}_{who}',1),'assert_mana',player=who,color=1),0)
        self.assertEqual(self.last(self.case('Cabal Stronghold','dynamic_count_0',1),'assert_mana',player=0,color=1),5)
    def test_black_grave_count_is_intersection_of_owner_zone_color_and_creature(self):
        for who in (0,1):
            self.assertEqual(self.last(self.case('Crypt of Agadeem',f'payment_{who}',2),'assert_mana',player=who,color=1),3)
            for mode in ('minimal_count','wrong_owner','contrary_kind'):self.assertEqual(self.last(self.case('Crypt of Agadeem',f'{mode}_{who}',2),'assert_mana',player=who,color=1),0)
        c=self.case('Crypt of Agadeem','dynamic_count_1',2);self.assertIn(dict(op='destroy',ref='black_live'),c['steps']);self.assertEqual(self.last(c,'assert_mana',player=1,color=1),7)
    def test_devotion_uses_symbols_including_hybrid_and_excludes_opponent_and_generic(self):
        self.assertEqual(m.devotion(self.oracle['Scion of Glaciers'],2),2);self.assertEqual(m.devotion(self.oracle["Judge's Familiar"],2),1);self.assertEqual(m.devotion(self.oracle["Judge's Familiar"],5),1);self.assertEqual(m.devotion(self.oracle['Scion of Glaciers'],3),0)
        for mode,color,expected in [('payment',2,3),('minimal_count',2,0),('partial_count',2,2),('wrong_owner',2,0),('color_g',3,1),('color_w',5,1),('color_b',1,0),('dynamic_count',2,5)]:self.assertEqual(self.last(self.case('Nykthos, Shrine to Nyx',mode+'_0',1),'assert_mana',player=0,color=color),expected)
    def test_wirewood_global_elf_count_real_cast_and_fresh_tap_denial(self):
        c=self.case('Wirewood Channeler');self.assertIn(dict(op='cast',ref='source',value=1),c['steps']);self.assertEqual(self.last(c,'require_card',ref='source',property='power'),2);self.assertEqual(self.last(c,'require_card',ref='source',property='toughness'),2);self.assertEqual(self.last(c,'assert_mana',player=0,color=2),3)
        self.assertEqual(self.last(self.case('Wirewood Channeler','wrong_owner_1'),'assert_mana',player=1,color=2),3);self.assertEqual(self.last(self.case('Wirewood Channeler','contrary_kind_0'),'assert_mana',player=0,color=2),2)
        c=self.case('Wirewood Channeler','fresh_1');self.assertEqual(self.last(c,'require_can_activate',ref='source'),0);self.assertEqual(self.last(c,'assert_card',ref='source',property='sick'),1);self.assertFalse(any(s['op']=='activate' and s['ref']=='source' for s in c['steps']))
    def test_xxx_cost_probe_keeps_printed_six_entry_two_and_measured_budget_twelve(self):
        for who in (0,1):
            c=self.case('Astral Cornucopia',f'xxx_cost_probe_{who}',1);ss=c['steps'];i=next(i for i,s in enumerate(ss) if s['op']=='cast' and s['ref']=='source')
            self.assertIn(dict(op='set_mana',player=who,color=0,value=12),ss[:i]);self.assertIn(dict(op='choose_x',player=who,value=2),ss[:i]);self.assertEqual(self.last(c,'assert_mana',player=who,color=0),6);self.assertEqual(self.last(c,'assert_counter',ref='source',counter_kind='charge'),2)
            self.assertEqual(c['_recipe']['entry_receipt'],dict(declared_value=2,printed_total=6,kind='x'));self.assertFalse(any(s['op']=='activate' and s['ref']=='source' for s in ss));self.assertFalse(any(s['op']=='set_mana' and s['player']==who for s in ss[i+1:]))
        c=self.case('Astral Cornucopia','payment_0',1);self.assertIn(dict(op='set_mana',player=0,color=0,value=8),c['steps'])
    def test_multikicker_real_optional_payments_and_declined_extra_budget(self):
        c=self.case('Everflowing Chalice','entry_three_0',2);ss=c['steps'];i=next(i for i,s in enumerate(ss) if s['op']=='cast');self.assertEqual([s['value'] for s in ss[:i] if s['op']=='choose_dialog'],[0,0,0]);self.assertEqual(self.last(c,'assert_counter',ref='source',counter_kind='charge'),3);self.assertEqual(self.last(c,'assert_mana',player=0,color=0),3)
        c=self.case('Everflowing Chalice','entry_decline_0',2);ss=c['steps'];i=next(i for i,s in enumerate(ss) if s['op']=='cast');self.assertEqual([s['value'] for s in ss[:i] if s['op']=='choose_dialog'],[0,0,1]);self.assertIn(dict(op='assert_mana',player=0,color=0,value=2),ss[i+1:]);self.assertEqual(c['_recipe']['entry_receipt']['printed_total'],4)
    def test_full_xxx_calibration_has_measured_actor_intents_true_fee_and_residual_before_fixture_budget(self):
        p=m.parse(self.oracle['Astral Cornucopia'],1)
        for variant in m.variants(p):
            c=self.case('Astral Cornucopia',variant,1);ss=c['steps'];who=int(variant[-1]);x=c['_recipe']['entry_receipt']['declared_value'];i=next(i for i,s in enumerate(ss) if s['op']=='cast')
            self.assertEqual([s['value'] for s in ss[:i] if s['op']=='choose_x'],[x]*(2 if who==0 else 1));self.assertEqual(c['_recipe']['entry_receipt']['printed_total'],3*x)
            budget=12 if variant.startswith('xxx_cost_probe') else 4*x
            self.assertEqual([s['value'] for s in ss[:i] if s['op']=='set_mana' and s['player']==who and s['color']==0][-1],budget)
            after=next((j for j in range(i+1,len(ss)) if ss[j]['op']=='set_mana' and ss[j]['player']==who),len(ss))
            self.assertIn(dict(op='assert_mana',player=who,color=0,value=budget-3*x),ss[i+1:after]);self.assertIn(dict(op='assert_counter',ref='source',counter_kind='charge',value=x),ss[i+1:after]);self.assertFalse(any(s['op'] in ('choose_mana_source','choose_cancel_mana') for s in ss))
        for v in ('entry_zero_0','entry_zero_1'):
            c=self.case('Astral Cornucopia',v,1);self.assertEqual(c['_recipe']['entry_receipt']['printed_total'],0);self.assertEqual(self.last(c,'assert_counter',ref='source',counter_kind='charge'),0)
    def test_measured_cast_funding_never_becomes_an_oracle_fee_or_zero_x_padding(self):
        d=self.declaration('Astral Cornucopia','payment_0',1)
        for value in (3,True,4.0,8):
            data=copy.deepcopy(m.pins());data['sources']['Astral Cornucopia']['casting_budget_x']=value
            with patch.object(m,'pins',return_value=data):
                with self.assertRaises(ValueError):m.spec(d,self.oracle['Astral Cornucopia'])
    def test_actual_zero_mana_branch_omits_color_only_and_keeps_all_tap_effect_observers(self):
        for who in (0,1):
            c=self.case('Astral Cornucopia',f'entry_zero_{who}',1);ss=c['steps']
            self.assertEqual([s['value'] for s in ss if s['op']=='choose_x'],[0]*(2 if who==0 else 1));self.assertFalse(any(s['op']=='choose_color' for s in ss));self.assertIn(dict(op='activate',ref='source',value=1),ss)
            self.assertEqual(self.last(c,'assert_card',ref='source',property='tapped'),1);self.assertEqual(self.last(c,'assert_counter',ref='source',counter_kind='charge'),0);self.assertEqual(self.last(c,'assert_mana_total',player=who),0)
        for who in (0,1):self.assertTrue(any(s['op']=='choose_color' for s in self.case('Astral Cornucopia',f'payment_{who}',1)['steps']))
    def test_opt_in_xxx_numeric_helper_probe_is_exact_and_preserves_oracle_fee(self):
        c=self.case('Astral Cornucopia','xxx_cost_probe_0',1);ss=c['steps'];i=next(i for i,s in enumerate(ss) if s['op']=='cast')
        self.assertEqual(ss[i-2:i],[dict(op='choose_x',player=0,value=2),dict(op='choose_x',player=0,value=2)])
        self.assertEqual(sum(s['op']=='choose_x' for s in ss),2);self.assertFalse(any(s['op'] in ('choose_mana','choose_cancel_mana','choose_mana_source') for s in ss))
        self.assertEqual(c['_recipe']['entry_receipt'],dict(declared_value=2,printed_total=6,kind='x'));self.assertEqual(self.last(c,'assert_mana',player=0,color=0),6);self.assertEqual(self.last(c,'assert_counter',ref='source',counter_kind='charge'),2)
        for v in ('xxx_cost_probe_1','payment_0','entry_one_0','entry_zero_0'):
            other=self.case('Astral Cornucopia',v,1);self.assertEqual(sum(s['op']=='choose_x' for s in other['steps']),2 if v.endswith('_0') else 1);self.assertFalse(any(s['op']=='choose_mana' for s in other['steps']))
        d=self.declaration('Astral Cornucopia','xxx_cost_probe_0',1)
        for binding in ({'1':{'xxx_cost_probe_1':['declared','declared']}},{'1':{'xxx_cost_probe_0':[0,'declared']}},{'1':{'payment_0':['declared','declared']}},{'1':{'xxx_cost_probe_0':['declared']}}):
            data=copy.deepcopy(m.pins());data['sources']['Astral Cornucopia']['casting_x_probe_inputs']=binding
            with patch.object(m,'pins',return_value=data):
                with self.assertRaises(ValueError):m.spec(d,self.oracle['Astral Cornucopia'])
        data=copy.deepcopy(m.pins());data['sources']['Astral Cornucopia']['casting_pool_clicks']={'1':{'xxx_cost_probe_0':[0]}}
        with patch.object(m,'pins',return_value=data):
            with self.assertRaises(ValueError):m.spec(d,self.oracle['Astral Cornucopia'])
    def test_chalice_zero_ai_policy_is_separate_from_real_zero_cost_cast_and_oracle_tap_legality(self):
        c=self.case('Everflowing Chalice','entry_zero_1',2);self.assertEqual(self.last(c,'can_activate',ref='source'),0);self.assertIs(c['_recipe']['raw_ai_availability']['oracle_legal_activation'],True);self.assertIn(dict(op='cast',ref='source',value=1),c['steps']);self.assertEqual(c['_recipe']['entry_receipt']['printed_total'],0);self.assertEqual(self.last(c,'assert_counter',ref='source',counter_kind='charge'),0)
        c=self.case('Everflowing Chalice','entry_zero_0',2);self.assertIn(dict(op='activate',ref='source',value=1),c['steps']);self.assertEqual(self.last(c,'assert_mana',player=0,color=0),0)
    def test_source_input_policy_cannot_normalize_oracle_legality_or_x_cost(self):
        n='Everflowing Chalice';d=self.declaration(n,'entry_zero_1',2)
        for key,value in [('oracle_legal_activation',False),('raw_value',False),('action','activate')]:
            data=copy.deepcopy(m.pins());data['sources'][n]['raw_ai_zero_query'][key]=value
            with patch.object(m,'pins',return_value=data):
                with self.assertRaises(ValueError):m.spec(d,self.oracle[n])
        n='Astral Cornucopia';d=self.declaration(n,'xxx_cost_probe_0',1);data=copy.deepcopy(m.pins());data['sources'][n]['casting_x_inputs']['0']=[0,'declared']
        with patch.object(m,'pins',return_value=data):
            with self.assertRaises(ValueError):m.spec(d,self.oracle[n])
    def test_counter_amount_recomputed_after_real_vorel_cost_and_resolution(self):
        c=self.case('Everflowing Chalice','dynamic_count_0',2);ss=c['steps'];i=next(i for i,s in enumerate(ss) if s['op']=='activate' and s['ref']=='counter_doubler')
        self.assertEqual(ss[i-1],dict(op='choose_target',player=1,ref='source'));self.assertIn(dict(op='assert_stack',value=1),ss[i:i+3]);self.assertEqual(self.last(c,'assert_counter',ref='source',counter_kind='charge'),4);self.assertEqual(self.last(c,'assert_mana',player=0,color=0),6);self.assertEqual(self.last(c,'assert_mana',player=1,color=2),0);self.assertEqual(self.last(c,'assert_mana',player=1,color=3),0)
    def test_land_tapped_native_observed_then_actual_twiddle_not_land_spell(self):
        c=self.case('Cloudpost');ss=c['steps'];self.assertIn(dict(op='require_card',ref='source',property='tapped',value=1),ss);self.assertIn(dict(op='cast',ref='entry_untap',value=1),ss);self.assertFalse(any(s['op']=='cast' and s['ref']=='source' for s in ss));self.assertEqual(self.last(c,'assert_card',ref='source',property='tapped'),1)
        c=self.case('Cloudpost','entry_tapped_0');self.assertEqual(self.last(c,'require_can_activate',ref='source'),0);self.assertFalse(any(s['op']=='cast' for s in c['steps']))
    def test_paid_repeat_uses_initial_real_generic_wallet_without_post_effect_pool_mutation(self):
        for n,a in [('Cabal Stronghold',1),('Crypt of Agadeem',2),('Nykthos, Shrine to Nyx',1)]:
            c=self.case(n,'dynamic_count_0',a);ss=c['steps'];i=self.activate(c);self.assertFalse(any(s['op']=='set_mana' for s in ss[i+1:]));self.assertEqual(sum(s['op']=='activate' and s['ref']=='source' for s in ss),2);self.assertEqual(self.last(c,'assert_mana',player=0,color=0),0)
    def test_all_printed_free_siblings_actually_execute_and_short_budget_query_is_scoped(self):
        for n,a,color in [('Cabal Stronghold',0,0),('Crypt of Agadeem',1,1),('Nykthos, Shrine to Nyx',0,0)]:
            c=self.case(n,'payment_1',a);self.assertIn(dict(op='activate',ref='source',value=1),c['steps'])
            if n=='Cabal Stronghold':self.assertNotIn(dict(op='choose_dialog',player=1,value=0),c['steps']);self.assertEqual(self.last(c,'assert_mana',player=1,color=0),1)
            else:self.assertIn(dict(op='choose_dialog',player=1,value=0),c['steps']);self.assertEqual(self.last(c,'assert_mana',player=1,color=color),m.pins()['sources'][n]['fixed_sibling_wallet']+1 if color==0 else 1)
        c=self.case('Cabal Stronghold','short_budget_query_0',1);self.assertEqual(self.last(c,'require_can_activate',ref='source'),1);self.assertFalse(any(s['op']=='activate' and s['ref']=='source' for s in c['steps']));self.assertTrue(any('unselected paid counted-mode' in g for g in c['_recipe']['untested_components']))
    def test_nykthos_actual_fixed_palette_intent_stays_C_and_preserves_unfunded_path(self):
        for who in (0,1):
            c=self.case('Nykthos, Shrine to Nyx',f'repeat_untap_{who}',0);ss=c['steps'];acts=[i for i,s in enumerate(ss) if s['op']=='activate' and s['ref']=='source']
            self.assertEqual(len(acts),2)
            for i in acts:self.assertEqual(ss[i-2:i],[dict(op='choose_dialog',player=who,value=0),dict(op='choose_color',player=who,value=0)])
            self.assertEqual(c['_recipe']['oracle_amount_expression'],dict(kind='fixed',color=0,cost=0));self.assertEqual(self.last(c,'assert_mana',player=who,color=0),4)
            c=self.case('Nykthos, Shrine to Nyx',f'fixed_zero_budget_{who}',0);self.assertFalse(any(s['op']=='choose_color' for s in c['steps']));self.assertEqual(self.last(c,'assert_mana',player=who,color=0),1)
    def test_cabal_paid_AI_omits_only_unoffered_source_mode_and_keeps_real_cost_count(self):
        c=self.case('Cabal Stronghold','dynamic_count_1',1);ss=c['steps'];acts=[i for i,s in enumerate(ss) if s['op']=='activate' and s['ref']=='source'];self.assertEqual(len(acts),2)
        for i in acts:self.assertEqual(ss[i-1],dict(op='require_can_activate',ref='source',value=1))
        self.assertEqual(c['_recipe']['oracle_amount_expression'],dict(kind='basic_swamps',color=1,cost=3));self.assertEqual(self.last(c,'assert_mana',player=1,color=1),5);self.assertEqual(self.last(c,'assert_mana',player=1,color=0),0)
        c=self.case('Cabal Stronghold','payment_0',1);i=self.activate(c);self.assertEqual(c['steps'][i-1],dict(op='choose_dialog',player=0,value=1))
    def test_cabal_free_AI_isolates_affordability_with_real_tapped_lands_and_wallet_zero(self):
        for v,total in [('payment_1',1),('repeat_untap_1',2),('spend_1',0)]:
            c=self.case('Cabal Stronghold',v,0);ownlands=[s for s in c['setup'] if s['player']==1 and s['zone']=='battlefield' and 'Land' in self.oracle[s['name']]['type_line'] and s['ref']!='source']
            self.assertEqual(sum(s['name']=='Swamp' for s in ownlands),3);self.assertTrue(all(s['state']==['tapped'] for s in ownlands));self.assertEqual(next(s['state'] for s in c['setup'] if s['ref']=='source'),['untapped']);self.assertFalse(any(s['op']=='choose_dialog' and s.get('player')==1 for s in c['steps']));self.assertEqual(self.last(c,'assert_mana',player=1,color=0),total)
            i=self.activate(c);self.assertFalse(any(s['op']=='set_mana' and s['player']==1 and s['value']>0 for s in c['steps'][:i]));self.assertEqual(c['_recipe']['oracle_amount_expression'],dict(kind='fixed',color=0,cost=0))
        for v in ('inactive_1','tapped_1'):
            c=self.case('Cabal Stronghold',v,0);self.assertTrue(all(s['state']==['untapped'] for s in c['setup'] if s['ref'].startswith('support_')))
        paid=self.case('Cabal Stronghold','payment_1',1);self.assertTrue(all(s['state']==['untapped'] for s in paid['setup'] if s['ref'].startswith('support_')))
    def test_fixture_presentation_extensions_are_exact_and_cannot_change_cost_or_actor(self):
        for n,field,value in [('Nykthos, Shrine to Nyx','fixed_output_choices',{'0':{'fixed_zero_budget_0':0}}),('Cabal Stronghold','ai_mode_dialog',True),('Cabal Stronghold','ai_free_fixture',{'variants':['payment_0'],'floating_colorless':0,'other_own_lands_tapped':True})]:
            data=copy.deepcopy(m.pins());data['sources'][n][field]=value;d=self.declaration(n)
            with patch.object(m,'pins',return_value=data):
                with self.assertRaises(ValueError):m.spec(d,self.oracle[n])
    def test_opposing_legend_copy_and_source_stats_remain_actual_independent_entities(self):
        c=self.case('Nykthos, Shrine to Nyx','opponent_legend_0',1);self.assertEqual(next(s['player'] for s in c['setup'] if s['ref']=='other_legend'),1);self.assertEqual(self.last(c,'require_card',ref='other_legend',property='in_play'),1)
    def test_urza_followers_produce_printed_two_only_for_own_complete_trio(self):
        for n in ("Urza's Mine","Urza's Power Plant"):
            for who in (0,1):
                self.assertEqual(self.last(self.case(n,f'payment_{who}'),'assert_mana',player=who,color=0),2)
                self.assertEqual(self.last(self.case(n,f'wrong_owner_{who}'),'assert_mana',player=who,color=0),1)
                self.assertEqual(self.last(self.case(n,f'contrary_kind_{who}'),'assert_mana',player=who,color=0),1)
                self.assertEqual(self.last(self.case(n,f'dynamic_count_{who}'),'assert_mana',player=who,color=0),3)
    def test_fixed_green_devotion_counts_self_matching_and_hybrid_but_not_foreign_or_blue(self):
        n="Karametra's Acolyte"
        for who in (0,1):
            for mode,total in [('payment',3),('minimal_count',1),('partial_count',2),('wrong_owner',1),('contrary_kind',2),('dynamic_count',5)]:
                c=self.case(n,f'{mode}_{who}');self.assertEqual(self.last(c,'assert_mana',player=who,color=3),total);self.assertFalse(any(s['op']=='choose_color' for s in c['steps']))
            c=self.case(n,f'payment_{who}');self.assertEqual(self.last(c,'require_card',ref='source',property='power'),1);self.assertEqual(self.last(c,'require_card',ref='source',property='toughness'),4)
        self.assertEqual(m.devotion(self.oracle['Elvish Hexhunter'],3),1);self.assertEqual(m.devotion(self.oracle['Elvish Hexhunter'],5),1)
        for s in self.case(n)['setup']:self.assertNotIn('Enchantment',self.oracle[s['name']]['type_line'])
    def test_ally_scope_excludes_foreign_copies_and_witnesses_have_no_entry_triggers(self):
        n='Harabaz Druid'
        for who in (0,1):
            for mode,total in [('payment',3),('minimal_count',1),('partial_count',2),('wrong_owner',1),('contrary_kind',2),('dynamic_count',5)]:self.assertEqual(self.last(self.case(n,f'{mode}_{who}'),'assert_mana',player=who,color=2),total)
            c=self.case(n,f'wrong_owner_{who}');self.assertTrue(any(s['ref']=='foreign_ally' and s['player']==1-who for s in c['setup']));self.assertEqual(self.last(c,'require_card',ref='source',property='power'),0);self.assertEqual(self.last(c,'require_card',ref='source',property='toughness'),1)
        self.assertNotIn('Whenever',self.oracle[n]['oracle_text']);self.assertNotIn('enters',self.oracle[n]['oracle_text'])
    def test_subtype_count_uses_printed_permanent_scope_not_creature_only(self):
        # Read-only Oracle calculation; this triggered enchantment is never
        # enrolled as an engine fixture or claimed as runtime coverage.
        n='Prowess of the Fair';self.assertIn('Elf',self.oracle[n]['type_line']);self.assertNotIn('Creature',self.oracle[n]['type_line'])
        entities={'printed_kindred':dict(name=n,player=1,zone='battlefield')}
        self.assertEqual(m.mana_amount(dict(kind='elves'),entities,self.oracle,0,2),1)
    def test_ready_and_resolved_snapshots_replay_correct_phase_and_strict_state(self):
        c=self.case('Wirewood Channeler','ready_snapshot_1');self.assertEqual(sum(s['op']=='activate' and s['ref']=='source' for s in c['steps']),2);self.assertEqual(c['steps'][-1],dict(op='assert_recorded_state'));self.assertIn(dict(op='ai_roundtrip'),c['steps'])
        c=self.case('Everflowing Chalice','resolved_snapshot_0',2);self.assertEqual(sum(s['op']=='activate' and s['ref']=='source' for s in c['steps']),1);self.assertEqual(c['steps'][-1],dict(op='assert_recorded_state'))
    def test_every_generated_fixture_has_predeclared_refs_and_no_counter_or_history_setter(self):
        count=0
        for n,s in m.pins()['sources'].items():
            for a in s['abilities']:
                p=m.parse(self.oracle[n],int(a));d=self.declaration(n,a=int(a));vs=m.variants(p);d['variants']=vs;m.spec(d,self.oracle[n])
                for c in m.expand(n,s['csvid'],d,p,vs,self.rows,self.oracle):
                    count+=1;refs={s['ref'] for s in c['setup']};self.assertTrue(all(s['ref'] in refs for s in c['steps'] if s['op']=='choose_target'));self.assertFalse(any(s['op'] in ('set_counter','set_sick','set_type','set_info_slot','dispatch_event') for s in c['steps']));self.assertIn(dict(op='assert_choices_consumed'),c['steps']);self.assertTrue(any(s['op']=='assert_zone_multiplicity' for s in c['steps']))
        self.assertEqual(count,494)


if __name__=='__main__':unittest.main()
