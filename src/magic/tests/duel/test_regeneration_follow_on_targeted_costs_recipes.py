"""Guard the printed cost and the compound regeneration paths."""

from copy import deepcopy
import unittest

import regeneration_follow_on_targeted_costs_recipes as model


class TargetedCostGuards(unittest.TestCase):
    def setUp(self):
        pinned = model.profiles()
        pinned = pinned['sources'] | pinned['witnesses']
        self.oracle = {name: item['oracle'] for name, item in pinned.items()}
        self.rows = {name: item['registry'] for name, item in pinned.items()}

    def build(self, name, mode, seat, board=model.Compound):
        game = board(self.rows[name]['csvid'], self.oracle[name], seat, mode, self.rows, self.oracle)
        game.build()
        return game

    def test_full_printed_record_and_forest_subtype_are_pinned(self):
        model.validate_witnesses(self.oracle, self.rows)
        corrupted = deepcopy(self.oracle)
        corrupted['Village Elder']['oracle_text'] = corrupted['Village Elder']['oracle_text'].replace('a Forest', 'a land')
        with self.assertRaises(ValueError):
            model.validate_witnesses(corrupted, self.rows)
        for seat in (0, 1):
            game = self.build('Village Elder', 'from_forest', seat, model.Duel)
            self.assertIn(dict(op='activate', ref='fodder', value=1), game.steps)
            self.assertEqual(game.cards['fodder']['zone'], 'graveyard')

    def test_barons_qualifying_damage_requires_a_death_this_turn(self):
        for seat in (0, 1):
            grow = self.build('Baron Sengir', 'grow', seat)
            later = self.build('Baron Sengir', 'after_turn', seat)
            self.assertIn(dict(op='assert_card', ref='source', property='power', value=7), grow.steps)
            self.assertNotIn(dict(op='assert_card', ref='source', property='power', value=7), later.steps)
            self.assertEqual(grow.cards['victim']['zone'], later.cards['victim']['zone'])

    def test_cube_payment_follows_combat_and_is_consumed(self):
        game = self.build("Delif's Cube", 'consumed', 1)
        ops = [(step['op'], step.get('ref')) for step in game.steps]
        self.assertLess(ops.index(('combat_damage', None)), ops.index(('choose_regeneration', 'source')))
        self.assertIn(('cast', 'second_removal'), ops)
        self.assertIn(dict(op='assert_unused_choices', value=1), game.steps)
        self.assertEqual(game.life, [20, 20])
        second = ops.index(('cast', 'second_removal'))
        self.assertEqual(ops[second - 1], ('choose_target', 'defender'))
        self.assertEqual(game.cards['victim']['zone'], 'battlefield')

    def test_matrix_and_licid_real_cost_and_type_receipts(self):
        for seat in (0, 1):
            matrix = self.build('Life Matrix', 'own_upkeep', seat)
            upkeep = matrix.steps.index(dict(op='upkeep_step', player=seat))
            activation = matrix.steps.index(dict(op='activate', ref='source', value=1))
            costs = matrix.steps[upkeep + 1:activation]
            self.assertEqual([step['ref'] for step in costs if step['op'] == 'activate'],
                             ['mana_0', 'mana_1', 'mana_2', 'mana_3'])
            self.assertFalse(any(step['op'] == 'turn' for step in costs))
            self.assertEqual([step['color'] for step in costs if step['op'] == 'choose_mana'],
                             [3, 3, 3, 3] if seat == 0 else [])
            self.assertEqual([step['value'] for step in matrix.steps if step['op'] == 'assert_card'
                              and step['ref'] == 'observer' and step['property'] == 'in_play'], [1, 0])
        licid = self.build('Nurturing Licid', 'revert', 1)
        self.assertEqual([step['value'] for step in licid.steps if step['op'] == 'can_target_creature'], [1, 0, 1])
