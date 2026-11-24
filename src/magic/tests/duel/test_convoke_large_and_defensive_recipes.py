"""Guard actual large Convoke payment and distinct defensive rider receipts."""
from collections import Counter
import json
import unittest

from generate_cases import registry
import convoke_large_and_defensive_recipes as model


class LargeConvokeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        data = model.PROFILE.parent
        cls.oracle = json.loads((data / 'oracle.json').read_bytes())['cards']
        cls.rows = {row['name']: row for row in registry() if row['kind'] != 'internal'}
        cls.homes = {}
        for name, pin in model.profiles()['sources'].items():
            home, = (data / 'cases/cards').glob(f"*/{pin['registry']['csvid']:05d}-*.json")
            cls.homes[name] = json.loads(home.read_bytes())['recipes'][0]

    def case(self, name, variant):
        declaration = self.homes[name]
        shape = model.spec(declaration, self.oracle[name])
        return model.build(name, self.rows[name]['csvid'], declaration, shape, variant, self.rows, self.oracle)

    def test_whole_records_and_large_payment(self):
        model.validate_witnesses(self.oracle, self.rows)
        for name, declaration in self.homes.items():
            shape = model.spec(declaration, self.oracle[name])
            self.assertEqual(declaration['variants'], model.variants(shape))
            with self.assertRaises(ValueError):
                model.spec(declaration, dict(self.oracle[name], oracle_text=model.CONVOKE))
        self.assertEqual(model.printed_cost('{10}{G}{G}{G}{W}{W}'), [10, 0, 0, 3, 0, 2])

    def test_real_cast_is_first_permission_gate(self):
        for name, declaration in self.homes.items():
            for variant in declaration['variants']:
                setup, steps = (self.case(name, variant)[key] for key in ('setup', 'steps'))
                cast = next(i for i, step in enumerate(steps) if step['op'] == 'cast' and step['ref'] == 'source')
                self.assertTrue(all(step['op'] not in ('can_cast', 'require_can_cast') for step in steps[:cast]))
                mode, owner = variant.rsplit('_', 1)
                self.assertEqual(steps[cast]['value'], int(mode != 'tapped_short'))
                helpers = [item for item in setup if item['ref'].startswith('helper_')]
                self.assertTrue(all(item['player'] == int(owner) and 'sick' in item['state'] for item in helpers))
                choices = [step for step in steps[:cast] if step['op'].startswith('choose')]
                if mode == 'paid':
                    self.assertEqual(choices, [dict(op='choose_dialog', player=int(owner), value=1)])
                elif mode == 'tapped_short':
                    self.assertTrue(all('tapped' in item['state'] for item in helpers))
                    self.assertEqual(choices, [])
                elif owner == '0':
                    self.assertEqual(Counter(s['ref'] for s in choices), Counter(item['ref'] for item in helpers))
                else:
                    self.assertEqual(choices, [])

    def test_riders_follow_real_cast(self):
        for owner in (0, 1):
            for name, spill in (('Autochthon Wurm', 1), ('Siege Wurm', 2)):
                steps = self.case(name, f'mixed_rider_{owner}')['steps']
                self.assertIn(dict(op='combat_damage'), steps)
                self.assertIn(dict(op='assert_life', player=1-owner, value=20-spill), steps)
            steps = self.case('Arboretum Elemental', f'mixed_rider_{owner}')['steps']
            self.assertIn(dict(op='can_target_creature', ref='foe_probe', other='source', value=0), steps)
            self.assertIn(dict(op='cast', ref='own_probe', value=1), steps)
            steps = self.case('Impervious Greatwurm', f'mixed_rider_{owner}')['steps']
            removal = steps.index(dict(op='cast', ref='foe_probe', value=1))
            self.assertIn(dict(op='assert_card', ref='source', property='in_play', value=1), steps[removal:])


if __name__ == '__main__':
    unittest.main()
