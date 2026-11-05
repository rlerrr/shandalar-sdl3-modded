import copy
import json
import unittest
from pathlib import Path
from generate_cases import registry
from recipes import expand,explicit_spec
from landcycling_recipes import variants


class LandcyclingRecipesTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards'];cls.rows={r['name']:r for r in registry()}

    def declaration(self,name='Absorb Vis',selected=None):
        cost,basic,types={'Absorb Vis':('{1}{B}',True,[]),'Chartooth Cougar':('{2}',False,['Mountain']),'Igneous Pouncer':('{2}',False,['Swamp','Mountain']),'Eternal Dragon':('{2}',False,['Plains']),'Ash Barrens':('{1}',True,[])}[name]
        return dict(family='landcycling',version=2,params=dict(cycling_cost=cost,basic=basic,types=types),oracle_text=self.oracle[name]['oracle_text'],variants=selected or ['search_0'])

    def cases(self,name='Absorb Vis',selected=None,d=None,oracle=None):
        return expand(name,self.rows[name]['csvid'],d or self.declaration(name,selected),self.oracle[name],self.rows,oracle_context=oracle or self.oracle)

    def test_full_pin_cost_and_types_must_match_the_printed_clause(self):
        d=self.declaration()
        for patch in (dict(oracle_text='changed'),dict(params={}),dict(params=dict(d['params'],cycling_cost='{2}')),dict(params=dict(d['params'],basic=1)),dict(params=dict(d['params'],types=['Forest'])),dict(params=dict(d['params'],cycling_cost='{X}'))):
            with self.assertRaises(ValueError):self.cases(d=dict(d,**patch))
        r=dict(self.oracle['Absorb Vis'],oracle_text=d['oracle_text']+'\nWhen you cycle this card, draw a card.')
        with self.assertRaises(ValueError):explicit_spec(dict(d,oracle_text=r['oracle_text']),r)

    def test_actual_hand_activation_discards_before_resolution(self):
        s=self.cases()[0]['steps'];resolve=next(i for i,x in enumerate(s) if x['op']=='resolve')
        for x in (dict(op='require_can_activate_hand',ref='source',value=1),dict(op='choose_dialog',player=0,value=1),dict(op='activate_hand',ref='source',value=1),dict(op='assert_zone_multiplicity',player=0,zone='graveyard',index=self.rows['Absorb Vis']['csvid'],value=1)):self.assertIn(x,s[:resolve])
        self.assertFalse(any(x['op']=='cast' for x in s))

    def test_basic_and_typed_union_filters_use_the_same_engine_observations(self):
        a=self.cases()[0]['steps'];b=self.cases('Igneous Pouncer')[0]['steps']
        self.assertIn(dict(op='assert_offered_legal_card',index=self.rows['Taiga']['csvid'],value=0),a)
        self.assertIn(dict(op='assert_offered_legal_card',index=self.rows['Taiga']['csvid'],value=1),b)
        self.assertIn(dict(op='assert_offered_legal_card',index=self.rows['Tundra']['csvid'],value=0),b)

    def test_discount_keeps_mandatory_colored_mana(self):
        s=self.cases(selected=['reducer_1'])[0]['steps']
        self.assertIn(dict(op='set_mana',player=1,color=0,value=0),s)
        self.assertIn(dict(op='set_mana',player=1,color=1,value=1),s)
        for c in range(6):self.assertIn(dict(op='assert_mana',player=1,color=c,value=0),s)
        denied=self.cases(selected=['reducer_missing_color_0'])[0]['steps']
        self.assertIn(dict(op='require_can_activate_hand',ref='source',value=0),denied)

    def test_tax_and_reduction_apply_to_the_correct_controller(self):
        for v,amount in [('tax_0',3),('opposing_tax_0',3),('two_taxes_0',5),('reducer_tax_0',1),('opposing_reducer_0',1)]:
            s=self.cases(selected=[v])[0]['steps'];self.assertIn(dict(op='set_mana',player=0,color=0,value=amount),s)

    def test_generic_cycling_can_be_free_after_reduction(self):
        s=self.cases('Chartooth Cougar',['reducer_0'])[0]['steps']
        for c in range(6):self.assertIn(dict(op='set_mana',player=0,color=c,value=0),s)
        self.assertIn(dict(op='activate_hand',ref='source',value=1),s)

    def test_cancel_does_not_discard_pay_or_search(self):
        s=self.cases(selected=['cancel_0'])[0]['steps']
        self.assertIn(dict(op='activate_hand',ref='source',value=0),s)
        self.assertIn(dict(op='assert_card',ref='source',property='in_hand',value=1),s)
        self.assertFalse(any(x['op']=='choose_zone_card' for x in s))

    def test_declining_the_search_still_discards_and_pays(self):
        s=self.cases(selected=['decline_1'])[0]['steps']
        self.assertIn(dict(op='choose_optional_zone',player=1,value=self.rows['Plains']['csvid']),s)
        self.assertIn(dict(op='assert_zone_multiplicity',player=1,zone='graveyard',index=self.rows['Absorb Vis']['csvid'],value=1),s)
        self.assertIn(dict(op='assert_forced_zone_choices',value=0),s)

    def test_pending_reclaim_changes_the_deck_not_the_ability(self):
        s=self.cases(selected=['reclaim_pending_0'])[0]['steps']
        self.assertIn(dict(op='cast',ref='reclaim',value=1),s)
        self.assertIn(dict(op='assert_stack',value=2),s)
        self.assertIn(dict(op='assert_offered_legal_card',index=self.rows['Absorb Vis']['csvid'],value=0),s)
        self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='library',index=self.rows['Absorb Vis']['csvid'],value=1),s)

    def test_exiling_the_discarded_card_does_not_cancel_the_search(self):
        s=self.cases(selected=['crypt_pending_1'])[0]['steps']
        self.assertIn(dict(op='activate',ref='crypt',value=1),s)
        self.assertIn(dict(op='assert_zone_multiplicity',player=1,zone='exile',index=self.rows['Absorb Vis']['csvid'],value=1),s)
        self.assertTrue(any(x['op']=='assert_offered_legal_card' for x in s))

    def test_two_hand_sources_and_snapshots_replay_real_activations(self):
        s=self.cases(selected=['two_sources_0'])[0]['steps'];self.assertEqual(sum(x['op']=='activate_hand' for x in s),2)
        s=self.cases(selected=['pending_snapshot_1'])[0]['steps']
        for op in ('capture','record_state','ai_roundtrip','restore','assert_recorded_state'):self.assertTrue(any(x['op']==op for x in s))

    def test_other_abilities_are_named_gaps_instead_of_being_cast(self):
        for name in ('Eternal Dragon','Ash Barrens','Chartooth Cougar'):
            c=self.cases(name)[0];self.assertTrue(c['_recipe']['untested_rules'])
            self.assertFalse(any(s['op']=='cast' for s in c['steps']))

    def test_timing_includes_opposing_turn_and_combat(self):
        a,b=self.cases(selected=['opposing_turn_0','combat_1'])
        self.assertIn(dict(op='turn',player=1,value=0),a['steps'])
        self.assertIn(dict(op='attack',ref='friendly',value=1),b['steps'])

    def test_variants_witness_pins_and_immutability(self):
        d=self.declaration();old=copy.deepcopy(d);self.cases(d=d);self.assertEqual(d,old)
        o=copy.deepcopy(self.oracle);o['Fluctuator']['oracle_text']='changed'
        with self.assertRaises(ValueError):self.cases(oracle=o)
        for vs in ([],['unknown_0'],['search_0','search_0']):
            with self.assertRaises(ValueError):self.cases(d=dict(d,variants=vs))
        p=explicit_spec(d,self.oracle['Absorb Vis']);self.assertEqual(len(variants(p)),len(set(variants(p))))


if __name__=='__main__':unittest.main()
