"""Mana recipe scope guards and Oracle-derived production/payment expectations."""
import copy,json,unittest
from pathlib import Path
from generate_cases import registry
from recipes import expand,explicit_spec
from mana_creature_recipes import ANY_COLOR,variants,output_pool,validate_witnesses

class ManaCreatureRecipesTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.registry={r['name']:r for r in registry()}
    def declaration(self,name='Elvish Mystic',outputs=None,cost='{T}',keys=None,form='fixed',selected=None):
        return dict(family='mana_creature',version=2,oracle_text=self.oracle[name]['oracle_text'],
                    params=dict(keywords=keys or [],mana_outputs=outputs or ['{G}'],activation_cost=cost,production=form),variants=selected or ['production_0'])
    def cases(self,name,d,registry=None,oracle=None):
        return expand(name,self.registry[name]['csvid'],d,self.oracle[name],registry or self.registry,oracle_context=oracle or self.oracle)
    def test_explicit_params_and_pin_fail_closed(self):
        d=self.declaration();self.cases('Elvish Mystic',d)
        for change in (dict(oracle_text='changed'),dict(params={}),dict(params=dict(d['params'],mana_outputs=['{R}'])),dict(params=dict(d['params'],activation_cost='{G}, {T}')),dict(params=dict(d['params'],production='any_color'))):
            with self.assertRaises(ValueError):self.cases('Elvish Mystic',dict(d,**change))
        for outputs in (['{X}'],['{S}'],['{2}'],['{G}{G}{G}{G}'],['{G}','{G}']):
            with self.assertRaises(ValueError):self.cases('Elvish Mystic',dict(d,params=dict(d['params'],mana_outputs=outputs)))

    def test_three_color_choice_keeps_each_ordinary_output(self):
        d=self.declaration('Druid of the Anima',['{R}','{G}','{W}'],form='choice',selected=['production_0','production_1','production_2'])
        cases=self.cases('Druid of the Anima',d)
        for case,color in zip(cases,(4,3,5)):
            self.assertIn(dict(op='choose_color',player=0,value=color),case['steps'])
        with self.assertRaises(ValueError):self.cases('Druid of the Anima',dict(d,params=dict(d['params'],mana_outputs=['{R}','{G}'])))
    def test_triggered_conditional_and_multimode_mana_are_not_certified(self):
        for name in ('Priest of Titania','Millikin','Heart Warden','Skyshroud Elf'):
            with self.assertRaises(ValueError):explicit_spec(self.declaration(name),self.oracle[name])
        token=dict(self.oracle['Elvish Mystic'],layout='token',type_line='Token Creature — Elf Druid')
        with self.assertRaises(ValueError):explicit_spec(self.declaration(),token)
    def test_any_color_has_five_distinct_outputs_and_no_colorless(self):
        d=self.declaration('Birds of Paradise',ANY_COLOR,keys=['Flying'],form='any_color',selected=['payment_4'])
        c=self.cases('Birds of Paradise',d)[0]
        self.assertIn(dict(op='choose_color',player=0,value=5),c['steps'])
        self.assertIn(dict(op='assert_mana',player=0,color=5,value=1),c['steps'])
        for color in range(5):self.assertIn(dict(op='assert_mana',player=0,color=color,value=0),c['steps'])
        with self.assertRaises(ValueError):self.cases('Birds of Paradise',dict(d,params=dict(d['params'],mana_outputs=['{C}']+ANY_COLOR)))
        record=dict(self.oracle['Elvish Mystic'],oracle_text='{T}: Add {C} or {G}.')
        choice=self.declaration(outputs=['{C}','{G}'],form='choice')
        choice['oracle_text']=record['oracle_text']
        c=expand('Elvish Mystic',self.registry['Elvish Mystic']['csvid'],choice,record,self.registry,oracle_context=self.oracle)[0]
        self.assertIn(dict(op='choose_color',player=0,value=0),c['steps'])
    def test_input_cost_and_three_colorless_output_are_exact(self):
        d=self.declaration('Apprentice Wizard',['{C}{C}{C}'],cost='{U}, {T}',selected=['payment_0','activation_wrong_color'])
        a,b=self.cases('Apprentice Wizard',d)
        self.assertIn(dict(op='assert_mana',player=0,color=0,value=3),a['steps'])
        self.assertIn(dict(op='assert_mana',player=0,color=2,value=0),a['steps'])
        self.assertIn(dict(op='can_activate',ref='source',value=0),b['steps'])
    def test_simultaneous_colors_do_not_prompt_a_choice(self):
        d=self.declaration('Knotvine Mystic',['{R}{G}{W}'],cost='{1}, {T}',selected=['payment_0','spend_0'])
        for c in self.cases('Knotvine Mystic',d):
            self.assertFalse(any(s['op']=='choose_color' for s in c['steps']))
        self.assertEqual(output_pool('{R}{G}{W}'),[0,0,0,1,1,1])
    def test_mana_resolves_immediately_and_tapped_repetition_is_rejected(self):
        c=self.cases('Elvish Mystic',self.declaration(selected=['repeat_denied_0']))[0]
        self.assertIn(dict(op='assert_stack',value=0),c['steps'])
        self.assertIn(dict(op='activate',ref='source',value=0),c['steps'])
        self.assertIn(dict(op='assert_mana',player=0,color=3,value=11),c['steps'])
    def test_sickness_includes_a_real_fresh_cast(self):
        d=self.declaration(selected=['fresh_activation','sick_activation','tapped_activation'])
        for c in self.cases('Elvish Mystic',d):self.assertIn(dict(op='can_activate',ref='source',value=0),c['steps'])
        fresh=self.cases('Elvish Mystic',d)[0]
        self.assertLess(fresh['steps'].index(dict(op='cast',ref='source',value=1)),fresh['steps'].index(dict(op='can_activate',ref='source',value=0)))
    def test_generated_mana_is_required_for_spending(self):
        c=self.cases('Elvish Mystic',self.declaration(selected=['spend_0']))[0]
        self.assertIn(dict(op='set_mana',player=0,color=3,value=0),c['steps'])
        self.assertIn(dict(op='cast',ref='spend_spell',value=1),c['steps'])
        self.assertIn(dict(op='assert_mana',player=0,color=3,value=1),c['steps'])
        self.assertIn(dict(op='assert_mana',player=0,color=3,value=0),c['steps'])
        generic=self.cases('Elvish Mystic',self.declaration(selected=['spend_generic_0']))[0]
        self.assertIn(dict(op='choose_mana',player=0,color=3),generic['steps'])
    def test_real_untapping_and_opponent_ownership(self):
        c=self.cases('Elvish Mystic',self.declaration(selected=['untap_repeat_0']))[0]
        self.assertIn(dict(op='cast',ref='untap_spell',value=1),c['steps'])
        self.assertEqual(sum(s==dict(op='activate',ref='source',value=1) for s in c['steps']),2)
        self.assertIn(dict(op='assert_card',ref='ally',property='tapped',value=0),c['steps'])
        self.assertIn(dict(op='assert_card',ref='enemy',property='tapped',value=1),c['steps'])
    def test_snapshot_and_both_populated_zones(self):
        c=self.cases('Elvish Mystic',self.declaration(selected=['mana_snapshot_0']))[0]
        for op in ('capture','record_state','ai_roundtrip','restore','assert_recorded_state'):self.assertIn(dict(op=op),c['steps'])
        for player in (0,1):self.assertIn(dict(op='assert_zone',player=player,zone='library',value=7),c['steps'])
    def test_witness_handlers_do_not_supply_expectations(self):
        d=self.declaration();r=copy.deepcopy(self.registry)
        for row in r.values():row['handler']='card_dummy'
        self.assertEqual(self.cases('Elvish Mystic',d),self.cases('Elvish Mystic',d,registry=r))
        o=copy.deepcopy(self.oracle);o['Vitalize']['oracle_text']='Untap all creatures.'
        with self.assertRaises(ValueError):validate_witnesses(o,self.registry)
        with self.assertRaises(ValueError):self.cases('Elvish Mystic',dict(d,variants=['production_1']))
        with self.assertRaises(ValueError):self.cases('Elvish Mystic',dict(d,resolve_dialogs=[0]))

if __name__=='__main__':unittest.main()
