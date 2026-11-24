"""Focused checks for distinct selection, signed effects, and partial resolution."""
import json
from pathlib import Path
import unittest

from generate_cases import registry
from recipes import cost_from_oracle, expand
import distinct_creature_stat_modifiers_recipes as family


class DistinctCreatureStatModifiersTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        root = Path(__file__).parent
        cls.oracle = json.loads((root/'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {row['name']:row for row in registry()}
        cls.cases, cls.decls = {}, {}
        for name, pin in family.pins()['sources'].items():
            csvid = pin['registry']['csvid']
            path, = (root/'data/cases/cards'/f'{csvid//100:03}').glob(f'{csvid:05d}-*.json')
            declaration = json.loads(path.read_text(encoding='utf8'))['recipes'][0]
            cls.decls[name] = declaration
            cls.cases[name] = {case['_recipe']['variant']:case for case in expand(
                name,csvid,declaration,cls.oracle[name],cls.rows,cls.oracle)}

    def steps(self, name, mode, owner):
        return self.cases[name][f'{mode}_{owner}']['steps']

    def test_whole_printed_opposites_and_distinct_target_clause(self):
        self.assertEqual(sum(map(len,self.cases.values())),136)
        for name, declaration in self.decls.items():
            r = self.oracle[name]
            family.spec(declaration,r)
            with self.assertRaises(ValueError):
                family.parameters(dict(r,oracle_text=r['oracle_text'].replace('Another target','That')))
            with self.assertRaises(ValueError):
                family.spec(dict(declaration,params=dict(declaration['params'],distinct=1)),r)
        bad = dict(self.rows, **{'Krosan Tusker':dict(self.rows['Krosan Tusker'],handler='card_dummy')})
        with self.assertRaises(ValueError):
            family.validate_witnesses(self.oracle,bad)

    def test_positive_and_negative_named_in_printed_order_for_either_controller(self):
        for name in self.cases:
            for owner in (0,1):
                for mode, target_owners in [('ordered_pair',[owner,1-owner]),('swapped_pair',[1-owner,owner]),
                                             ('own_pair',[owner,owner]),('opponent_pair',[1-owner,1-owner])]:
                    case = self.cases[name][f'{mode}_{owner}']
                    self.assertEqual([x['player'] for x in case['setup'] if x['ref'] in ('plus','minus')],target_owners)
                    if mode=='own_pair':
                        self.assertEqual([x['name'] for x in case['setup'] if x['ref'] in ('plus','minus')],['Krosan Tusker']*2)
                    steps = case['steps']
                    cast = steps.index(dict(op='cast',ref='source',value=1))
                    self.assertEqual(steps[cast-2:cast],[dict(op='choose_target',player=owner,ref=ref) for ref in ('plus','minus')])

    def test_singleton_and_mandatory_second_target_cancellation(self):
        for name in self.cases:
            for owner in (0,1):
                lone = self.steps(name,'singleton',owner)
                self.assertIn(dict(op='can_target_creature',ref='source',other='plus',value=1),lone)
                self.assertIn(dict(op='can_cast',ref='source',value=0),lone)
                self.assertFalse(any(s['op'] in ('cast','choose_target') for s in lone))
                scene = self.steps(name,'cancel_second',owner)
                cast = scene.index(dict(op='cast',ref='source',value=0))
                self.assertEqual(scene[cast-2:cast],[dict(op='choose_target',player=owner,ref='plus'),
                                                    dict(op='choose_cancel_target',player=owner)])
                for color, value in enumerate(cost_from_oracle(self.oracle[name])):
                    self.assertIn(dict(op='assert_mana',player=owner,color=color,value=value),scene[cast:])

    def test_one_illegal_target_does_not_remove_the_other_effect(self):
        for name, decl in self.decls.items():
            pp, pt = decl['params']['positive']
            np, nt = decl['params']['negative']
            for owner in (0,1):
                for mode, other, power, toughness in [('lost_plus','minus',4+np,4+nt),('lost_minus','plus',6+pp,5+pt),
                                                       ('reentered_plus','minus',4+np,4+nt)]:
                    steps = self.steps(name,mode,owner)
                    response = steps.index(dict(op='resolve_response'))
                    cleanup = steps.index(dict(op='cleanup'))
                    self.assertIn(dict(op='cast_response',ref='answer',value=1),steps[:response])
                    self.assertIn(dict(op='assert_stack',value=2),steps[:response])
                    self.assertEqual(steps[response+3],dict(op='assert_stack',value=0))
                    self.assertNotIn(dict(op='resolve'),steps[response:cleanup])
                    self.assertIn(dict(op='assert_card',ref=other,property='power',value=power),steps[response:cleanup])
                    self.assertIn(dict(op='assert_card',ref=other,property='toughness',value=toughness),steps[response:cleanup])
                    if mode=='reentered_plus':
                        self.assertIn(dict(op='bind_permanent',ref='plus',player=owner,value=6196),steps[response:cleanup])
                        self.assertIn(dict(op='assert_card',ref='plus',property='power',value=6),steps[response:cleanup])

    def test_zero_toughness_or_power_only_combat_and_cleanup(self):
        for name, dec in self.decls.items():
            mode = 'lethal_shrink' if dec['params']['negative'][1] else 'power_only_combat'
            for owner in (0,1):
                steps = self.steps(name,mode,owner)
                clean = steps.index(dict(op='cleanup'))
                if mode=='lethal_shrink':
                    self.assertIn(dict(op='assert_card',ref='minus',property='in_play',value=0),steps[:clean])
                    victim = family.LETHAL_WITNESSES[-dec['params']['negative'][1]]
                    self.assertIn(dict(op='assert_zone_multiplicity',player=1-owner,zone='graveyard',
                                       index=self.rows[victim]['csvid'],value=1),steps[clean:])
                else:
                    self.assertIn(dict(op='assert_card',ref='minus',property='power',value=0),steps[:clean])
                    self.assertIn(dict(op='attack',ref='minus',value=1),steps[:clean])
                    self.assertIn(dict(op='assert_life',player=owner,value=20),steps[:clean])
                self.assertIn(dict(op='assert_card',ref='plus',property='power',value=6),steps[clean:])

    def test_instants_go_off_turn_and_resolve_before_actual_pending_spell(self):
        for name, decl in self.decls.items():
            if decl['params']['timing']=='sorcery':
                self.assertFalse(any(v.startswith(('instant_response','offturn')) for v in self.cases[name]))
                self.assertEqual(self.steps(name,'ordered_pair',1)[0],dict(op='turn',player=1,value=0))
                continue
            for owner in (0,1):
                run = self.steps(name,'instant_response',owner)
                self.assertEqual(run[0],dict(op='turn',player=1-owner,value=0))
                response = run.index(dict(op='cast_response',ref='source',value=1))
                self.assertIn(dict(op='announce_spell',ref='precursor'),run[:response])
                result = run.index(dict(op='resolve_response'))
                cleanup = run.index(dict(op='cleanup'))
                self.assertIn(dict(op='assert_stack',value=2),run[response:result])
                self.assertEqual(run[result+3],dict(op='assert_stack',value=0))
                self.assertNotIn(dict(op='resolve'),run[result:cleanup])
                self.assertIn(dict(op='assert_card',ref='plus',property='damage',value=2),run[result:cleanup])

    def test_replay_has_a_paid_pending_parent_and_exact_tail(self):
        for name in self.cases:
            steps = self.steps(name,'pending_replay',0)
            cut = steps.index(dict(op='capture'))
            record = steps.index(dict(op='record_state'))
            restore = steps.index(dict(op='restore'))
            self.assertIn(dict(op='assert_stack',value=1),steps[:cut])
            self.assertEqual(steps[cut+1:record],steps[restore+1:-1])


if __name__ == '__main__':
    unittest.main()
