"""Self-pump expectations, legal fixtures, real stack actions, and scope guards."""
import copy,json,unittest
from pathlib import Path
from generate_cases import registry
from recipes import expand,explicit_spec
from pump_creature_recipes import variants

class PumpCreatureRecipesTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle=json.loads((Path(__file__).parent/'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.registry={r['name']:r for r in registry()}
    def declaration(self,name='Furnace Whelp',selected=None,**params):
        return dict(family='self_pump_creature',version=2,oracle_text=self.oracle[name]['oracle_text'],params=dict(keywords=['Flying'],power=1,toughness=0,activation_cost='{R}',**params),variants=selected or ['activation'])
    def cases(self,name,d,registry=None,oracle=None):
        return expand(name,self.registry[name]['csvid'],d,self.oracle[name],registry or self.registry,oracle_context=oracle or self.oracle)
    def test_pin_scope_parameters_and_limits_fail_closed(self):
        d=self.declaration()
        self.cases('Furnace Whelp',d)
        for change in (dict(oracle_text='changed'),dict(params=[]),dict(params=dict(d['params'],power=-1)),dict(params=dict(d['params'],power=5)),dict(params=dict(d['params'],activation_cost='{T}')),dict(params=dict(d['params'],activation_limit=True)),dict(activation_dialog=-1),dict(params=dict(d['params'],keywords=[]))):
            with self.assertRaises(ValueError):self.cases('Furnace Whelp',dict(d,**change))
        for name in ('Dragon Whelp','Serra Angel','Infantry Veteran'):
            with self.assertRaises(ValueError):explicit_spec(dict(d,oracle_text=self.oracle[name]['oracle_text']),self.oracle[name])
    def test_pending_activations_preserve_base_until_resolved(self):
        c=self.cases('Furnace Whelp',self.declaration(selected=['stacked_activations']))[0];s=c['steps']
        pending=s.index(dict(op='assert_stack',value=2));resolved=s.index(dict(op='resolve'))
        self.assertLess(pending,resolved)
        self.assertIn(dict(op='assert_card',ref='source',property='power',value=2),s[pending:resolved])
        self.assertIn(dict(op='assert_card',ref='source',property='power',value=4),s[resolved:])
        self.assertIn(dict(op='assert_mana_total',player=0,value=58),s)
    def test_tapped_and_fresh_nontapping_abilities_are_legal(self):
        tapped,fresh=self.cases('Furnace Whelp',self.declaration(selected=['tapped_activation','fresh_activation']))
        self.assertEqual(tapped['setup'][0]['state'],['tapped'])
        self.assertIn(dict(op='activate',ref='source',value=1),tapped['steps'])
        self.assertEqual(fresh['setup'][0]['zone'],'hand')
        s=fresh['steps'];self.assertLess(s.index(dict(op='cast',ref='source',value=1)),s.index(dict(op='activate',ref='source',value=1)))
        self.assertIn(dict(op='assert_card',ref='source',property='sick',value=1),s)
    def test_combat_and_lost_source_use_normal_actions(self):
        combat,lost=self.cases('Furnace Whelp',self.declaration(selected=['combat_activation','lost_source']))
        s=combat['steps'];self.assertLess(s.index(dict(op='finish_attackers')),s.index(dict(op='activate',ref='source',value=1)))
        self.assertIn(dict(op='assert_life',player=1,value=17),s)
        self.assertIn(dict(op='choose_priority',ref='response'),lost['steps'])
        self.assertIn(dict(op='assert_zone_contains',player=0,zone='hand',value=7059),lost['steps'])

    def test_fresh_haste_does_not_expect_the_engine_to_restrict_tapping(self):
        d=self.declaration('Pyre Charger',selected=['fresh_activation'])
        d['params']=dict(keywords=['Haste'],power=1,toughness=0,activation_cost='{R}')
        s=self.cases('Pyre Charger',d)[0]['steps']
        self.assertIn(dict(op='assert_card',ref='source',property='sick',value=0),s)
        self.assertIn(dict(op='activate',ref='source',value=1),s)
    def test_per_turn_limit_and_cleanup_reset(self):
        d=self.declaration('Vampire Bats',selected=['activation_limit','activation_limit_reset'],activation_limit=2);d['params']['activation_cost']='{B}'
        limit,reset=self.cases('Vampire Bats',d)
        self.assertEqual(sum(s['op']=='activate' for s in limit['steps']),2)
        self.assertIn(dict(op='can_activate',ref='source',value=0),limit['steps'])
        tail=reset['steps'][reset['steps'].index(dict(op='cleanup')):]
        self.assertIn(dict(op='can_activate',ref='source',value=1),tail)
        self.assertIn(dict(op='assert_mana_total',player=0,value=57),tail)
    def test_all_zones_and_snapshots_and_oracle_stats_are_independent(self):
        d=self.declaration(selected=['activation_snapshot','cast']);altered=copy.deepcopy(self.registry)
        for r in altered.values():r['handler']='card_dummy';r['mana_cost']='{99}'
        self.assertEqual(self.cases('Furnace Whelp',d),self.cases('Furnace Whelp',d,registry=altered))
        cases=self.cases('Furnace Whelp',d);snapshot=next(c for c in cases if c['_recipe']['variant']=='activation_snapshot')
        self.assertIn(dict(op='ai_roundtrip'),snapshot['steps']);self.assertIn(dict(op='assert_recorded_state'),snapshot['steps'])
        self.assertTrue(all(c['fixture']=='stocked_zones' for c in cases))
        for p in (0,1):
            self.assertIn(dict(op='assert_zone',player=p,zone='library',value=7),snapshot['steps'])
            self.assertIn(dict(op='assert_zone',player=p,zone='exile',value=2),snapshot['steps'])
    def test_multimode_does_not_assert_global_affordability_from_one_mode(self):
        d=self.declaration('Truefire Paladin');d['params']=dict(keywords=['Vigilance'],power=2,toughness=0,activation_cost='{R}{W}')
        self.assertNotIn('activation_wrong_color',variants(explicit_spec(d,self.oracle['Truefire Paladin'])))
        with self.assertRaises(ValueError):self.cases('Truefire Paladin',dict(d,variants=['activation_wrong_color']))

    def test_activation_menus_are_scoped_and_replayed_for_each_activation(self):
        d=self.declaration('Truefire Paladin',selected=['repeated_activations']);d['params']=dict(keywords=['Vigilance'],power=2,toughness=0,activation_cost='{R}{W}')
        d['activation_dialogs']={'repeated_activations':0}
        c=self.cases('Truefire Paladin',d)[0]
        self.assertEqual(c['steps'].count(dict(op='choose_dialog',player=0,value=0)),3)
        self.assertEqual(c['_recipe']['activation_dialog'],0)
        for bad in ({'other':0},{'repeated_activations':False},{'repeated_activations':9}):
            with self.assertRaises(ValueError):self.cases('Truefire Paladin',dict(d,activation_dialogs=bad))

    def test_expensive_repetitions_have_enough_generic_mana_without_color_choices(self):
        d=self.declaration('Kraul Warrior',selected=['repeated_activations','five_activations']);d['params']=dict(keywords=[],power=3,toughness=3,activation_cost='{5}{G}')
        three,five=self.cases('Kraul Warrior',d)
        for c,reserve,remaining in ((three,15,47),(five,25,45)):
            self.assertEqual(c['steps'][0],dict(op='set_mana',player=0,color=0,value=reserve))
            self.assertIn(dict(op='assert_mana_total',player=0,value=remaining),c['steps'])

    def test_wrong_payment_prompt_cancellation_keeps_correct_cost_and_effect_assertions(self):
        d=self.declaration('Windreaver',selected=['activation_payment']);d['params']=dict(keywords=['Flying'],power=0,toughness=1,activation_cost='{W}')
        d['activation_dialogs']={'activation_payment':1};d['activation_cancel_mana']=['activation_payment']
        c=self.cases('Windreaver',d)[0];s=c['steps']
        self.assertIn(dict(op='choose_cancel_mana',player=0),s)
        self.assertIn(dict(op='activate',ref='source',value=1),s)
        self.assertIn(dict(op='assert_mana_total',player=0,value=0),s)
        self.assertIn(dict(op='assert_card',ref='source',property='toughness',value=4),s)
        self.assertIn(dict(op='assert_choices_consumed'),s)
        with self.assertRaises(ValueError):self.cases('Windreaver',dict(d,activation_cancel_mana=['lost_source']))
        with self.assertRaises(ValueError):self.cases('Windreaver',dict(d,activation_cancel_mana=[{}]))

if __name__=='__main__':unittest.main()
