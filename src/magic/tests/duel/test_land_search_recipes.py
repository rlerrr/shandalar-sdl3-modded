import copy
import json
import unittest
from pathlib import Path
from generate_cases import registry
from recipes import expand,explicit_spec
from land_search_recipes import eligible,variants,LANDS


class LandSearchRecipesTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards'];cls.rows={r['name']:r for r in registry()}

    def declaration(self,name='Flooded Strand',selected=None):
        configs={
            'Flooded Strand':dict(types=['Plains','Island'],tap=True,sacrifice=True,pay_life=1),
            'Bad River':dict(types=['Island','Swamp'],tap=True,sacrifice=True,enters_tapped=True),
            'Rampant Growth':dict(mode='spell',basic=True,destination='battlefield_tapped'),
            'Into the North':dict(mode='spell',snow=True,destination='battlefield_tapped'),
            'Sylvan Scrying':dict(mode='spell',destination='hand'),
            'Expedition Map':dict(activation_cost='{2}',tap=True,sacrifice=True,destination='hand'),
            'Sakura-Tribe Elder':dict(basic=True,sacrifice=True,destination='battlefield_tapped'),
            "Journeyer's Kite":dict(basic=True,activation_cost='{3}',tap=True,destination='hand'),
            'Bant Panorama':dict(types=['Forest','Plains','Island'],basic=True,activation_cost='{1}',tap=True,sacrifice=True,destination='battlefield_tapped',mana_ability=True),
        }
        p=dict(mode='activated',types=[],basic=False,snow=False,destination='battlefield',activation_cost='',tap=False,sacrifice=False,pay_life=0,enters_tapped=False,mana_ability=False);p.update(configs[name])
        d=dict(family='land_search',version=2,params=p,oracle_text=self.oracle[name]['oracle_text'],variants=selected or ['search_0'])
        if p['mana_ability']:d['activation_dialogs']={'0':[1],'1':[]}
        return d

    def cases(self,name='Flooded Strand',selected=None,d=None,oracle=None):
        return expand(name,self.rows[name]['csvid'],d or self.declaration(name,selected),self.oracle[name],self.rows,oracle_context=oracle or self.oracle)

    def test_whole_text_pin_cost_and_scope_fail_closed(self):
        d=self.declaration()
        for patch in (dict(oracle_text='changed'),dict(params={}),dict(params=dict(d['params'],pay_life=True)),dict(params=dict(d['params'],types=['Forest'])),dict(params=dict(d['params'],types=['Plains','Plains'])),dict(params=dict(d['params'],basic=True)),dict(params=dict(d['params'],activation_cost='{1}'))):
            with self.assertRaises(ValueError):self.cases(d=dict(d,**patch))
        r=dict(self.oracle['Flooded Strand'],oracle_text=d['oracle_text']+'\nWhen this land enters, draw a card.')
        with self.assertRaises(ValueError):explicit_spec(dict(d,oracle_text=r['oracle_text']),r)

    def test_typed_search_includes_duals_and_excludes_wrong_types(self):
        p=explicit_spec(self.declaration(),self.oracle['Flooded Strand'])
        for name,allowed in [('Tundra',True),('Savannah',True),('Snow-Covered Island',True),('Taiga',False),('Forest',False),('Darksteel Citadel',False),('Grizzly Bears',False)]:self.assertEqual(eligible(self.oracle[name],p),allowed,name)

    def test_basic_and_snow_are_distinct_from_land_subtypes(self):
        basic=explicit_spec(self.declaration('Rampant Growth'),self.oracle['Rampant Growth']);snow=explicit_spec(self.declaration('Into the North'),self.oracle['Into the North'])
        self.assertTrue(eligible(self.oracle['Snow-Covered Forest'],basic));self.assertFalse(eligible(self.oracle['Taiga'],basic))
        self.assertTrue(eligible(self.oracle['Snow-Covered Forest'],snow));self.assertFalse(eligible(self.oracle['Forest'],snow))
        self.assertTrue(eligible(self.oracle['Scrying Sheets'],snow));self.assertFalse(eligible(self.oracle['Scrying Sheets'],basic))

    def test_actual_engine_filter_is_observed_for_the_entire_mixed_library(self):
        c=self.cases()[0];s=c['steps'];self.assertIn(dict(op='observe_search_filters',value=1),s)
        self.assertEqual(sum(x['op']=='assert_offered_legal_card' for x in s),20)
        for nm in ('Grizzly Bears','Giant Growth'):self.assertIn(dict(op='assert_offered_legal_card',index=self.rows[nm]['csvid'],value=0),s)

    def test_repeat_search_observations_allow_the_library_to_be_shuffled(self):
        s=self.cases(selected=['two_sources_0'])[0]['steps']
        self.assertFalse(any(x['op']=='assert_offered_card' for x in s))
        self.assertEqual(sum(x['op']=='assert_offered_legal_card' for x in s),40)

    def test_sacrifice_and_life_are_paid_before_search_resolution(self):
        s=self.cases()[0]['steps'];resolve=next(i for i,x in enumerate(s) if x['op']=='resolve');before=s[:resolve]
        self.assertIn(dict(op='assert_card',ref='source',property='in_play',value=0),before)
        self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='graveyard',index=self.rows['Flooded Strand']['csvid'],value=1),before)
        self.assertIn(dict(op='assert_life',player=0,value=19),s)

    def test_decline_preserves_library_and_still_pays_the_activation(self):
        s=self.cases(selected=['decline_1'])[0]['steps']
        self.assertIn(dict(op='assert_life',player=1,value=19),s)
        self.assertIn(dict(op='choose_optional_zone',player=1,value=self.rows['Plains']['csvid']),s)
        self.assertIn(dict(op='assert_forced_zone_choices',value=0),s)
        self.assertIn(dict(op='assert_zone',player=1,zone='library',value=21),s)

    def test_hand_search_does_not_put_a_permanent_on_the_battlefield(self):
        c=self.cases('Sylvan Scrying')[0];s=c['steps']
        self.assertFalse(any(x['op']=='bind_permanent_copy' for x in s))
        self.assertIn(dict(op='assert_zone_multiplicity',player=0,zone='hand',index=self.rows['Plains']['csvid'],value=2),s)

    def test_destination_tapping_and_typed_dual_receipts(self):
        c=self.cases('Rampant Growth')[0]
        self.assertIn(dict(op='assert_card',ref='found_1',property='tapped',value=1),c['steps'])
        c=self.cases(selected=['land_10_1'])[0]
        self.assertIn(dict(op='bind_permanent_copy',player=1,value=self.rows['Tundra']['csvid'],index=0,ref='found_1'),c['steps'])
        self.assertIn(dict(op='assert_card',ref='found_1',property='tapped',value=0),c['steps'])

    def test_exact_payment_and_tax_are_modeled_as_nonmana_costs(self):
        for name,v,cost in [('Expedition Map','payment_0',0),('Expedition Map','tax_1',16)]:
            s=self.cases(name,[v])[0]['steps'];self.assertIn(dict(op='assert_mana',player=int(v[-1]),color=0,value=cost),s)

    def test_separate_mana_ability_does_not_make_fetch_affordability_a_global_negative(self):
        d=self.declaration('Bant Panorama');p=explicit_spec(d,self.oracle['Bant Panorama'])
        self.assertNotIn('insufficient_mana_0',variants(p))
        self.assertIn(dict(op='choose_dialog',player=0,value=1),self.cases('Bant Panorama')[0]['steps'])

    def test_entry_tapped_and_ready_sources_have_separate_fixtures(self):
        a,b=self.cases('Bad River',['entry_tapped_0','search_0'])
        self.assertIn(dict(op='require_can_activate',ref='source',value=0),a['steps'])
        self.assertNotIn('state',next(c for c in a['setup'] if c['ref']=='source'))
        self.assertEqual(next(c for c in b['setup'] if c['ref']=='source')['state'],['untapped'])

    def test_duplicate_fetches_and_reusable_searches_preserve_multiplicity(self):
        for name,v in [('Flooded Strand','two_sources_0'),("Journeyer's Kite",'repeat_1')]:
            s=self.cases(name,[v])[0]['steps'];self.assertEqual(sum(x['op']=='choose_zone_card' for x in s),2)

    def test_snapshot_captures_paid_search_stack_not_an_untap_spell(self):
        s=self.cases('Bad River',['pending_snapshot_1'])[0]['steps'];cut=next(i for i,x in enumerate(s) if x['op']=='capture')
        self.assertIn(dict(op='activate',ref='source',value=1),s[:cut])
        self.assertNotIn(dict(op='resolve'),s[:cut])
        for op in ('record_state','ai_roundtrip','restore','assert_recorded_state'):self.assertTrue(any(x['op']==op for x in s))

    def test_spell_countering_leaves_search_and_library_untouched(self):
        s=self.cases('Rampant Growth',['countered_0'])[0]['steps']
        self.assertIn(dict(op='cast',ref='source',value=0),s)
        self.assertFalse(any(x['op']=='choose_zone_card' for x in s))

    def test_consumed_source_references_are_not_confused_with_reused_slots(self):
        s=self.cases()[0]['steps'];resolve=next(i for i,x in enumerate(s) if x['op']=='resolve')
        self.assertFalse(any(x['op']=='assert_card' and x['ref']=='source' for x in s[resolve+1:]))

    def test_witness_pins_variants_and_immutability(self):
        d=self.declaration();old=copy.deepcopy(d);self.cases(d=d);self.assertEqual(d,old)
        o=copy.deepcopy(self.oracle);o['Tundra']['type_line']='Land — Forest'
        with self.assertRaises(ValueError):self.cases(oracle=o)
        for vs in ([],['unknown_0'],['search_0','search_0']):
            with self.assertRaises(ValueError):self.cases(d=dict(d,variants=vs))


if __name__=='__main__':unittest.main()
