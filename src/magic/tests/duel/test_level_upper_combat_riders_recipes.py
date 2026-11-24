"""Guards for paid threshold and combat rider projections."""

from copy import deepcopy
import unittest

import level_upper_combat_riders_recipes as recipe


class LevelUpperCombatTests(unittest.TestCase):
    def setUp(self):
        pins = recipe.profiles()
        self.oracle = {name: pin['oracle'] for group in pins.values() for name, pin in group.items()}
        self.rows = {name: pin['registry'] for group in pins.values() for name, pin in group.items()}

    def scenario(self, name, mode):
        shape = recipe.classify(self.oracle[name])
        board = recipe.CombatLevelBoard(name, shape, mode, 0, self.rows, self.oracle)
        board.run()
        return board

    def test_full_oracle_and_variant_controls(self):
        recipe.validate_witnesses(self.oracle, self.rows)
        for name, pin in recipe.profiles()['sources'].items():
            with self.subTest(card=name):
                record = pin['oracle']
                model = recipe.classify(record)
                variants = recipe.variants(model)
                declaration = dict(family=recipe.FAMILY, version=2, params=model,
                                   oracle_text=record['oracle_text'], variants=variants)
                self.assertEqual(recipe.spec(declaration, record)['family'], recipe.FAMILY)
                changed = deepcopy(record)
                changed['oracle_text'] += '\nHaste'
                with self.assertRaises(ValueError):
                    recipe.classify(changed)
                with self.assertRaises(ValueError):
                    recipe.spec(declaration, changed)
                with self.assertRaises(ValueError):
                    recipe.spec(dict(declaration, variants=variants + [variants[0]]), record)
                bad_cost = dict(model, activation=[0] * 6)
                with self.assertRaises(ValueError):
                    recipe.spec(dict(declaration, params=bad_cost), record)

    def test_real_combat_outcomes_distinguish_level_bands(self):
        lower = self.scenario('Kargan Dragonlord', 'flight_middle')
        self.assertEqual([(op['ref'], op['value']) for op in lower.steps if op['op'] == 'block'],
                         [('ground', 0), ('flier', 1)])
        self.assertEqual(lower.life, [20, 20])
        self.assertEqual(self.scenario('Kargan Dragonlord', 'trample_upper').life, [20, 13])
        middle = self.scenario('Nirkana Cutthroat', 'deathtouch')
        self.assertEqual([middle.cards[k]['zone'] for k in ('source', 'wall')], ['graveyard'] * 2)
        upper = self.scenario('Nirkana Cutthroat', 'first_strike')
        self.assertEqual([upper.cards[k]['zone'] for k in ('source', 'wall')], ['battlefield', 'graveyard'])
        self.assertEqual(upper.cards['source']['damage'], 0)
        self.assertEqual(self.scenario('Zulaport Enforcer', 'nonblack_upper').life, [20, 15])
        self.assertEqual(self.scenario('Zulaport Enforcer', 'black_upper').life, [20, 20])

    def test_paid_pump_and_indestructibility_are_separate_riders(self):
        pump = self.scenario('Kargan Dragonlord', 'pump_offturn')
        accepted = [op for op in pump.steps if op['op'] == 'activate' and op['value'] == 1]
        self.assertEqual(len(accepted), 10)
        self.assertEqual(pump.level, 8)
        self.assertEqual(pump.source_stats(), (8, 8))
        self.assertIn({'op': 'assert_card', 'ref': 'source', 'property': 'power', 'value': 10}, pump.steps)
        lives = self.scenario('Transcendent Master', 'destroy_upper')
        self.assertEqual(lives.life, [29, 11])
        self.assertEqual(lives.cards['source']['zone'], 'battlefield')
        self.assertEqual(lives.cards['removal']['zone'], 'graveyard')
        exiled = self.scenario('Transcendent Master', 'exile_upper')
        self.assertEqual(exiled.life, [29, 20])
        self.assertEqual(exiled.cards['source']['zone'], 'exile')


if __name__ == '__main__':
    unittest.main()
