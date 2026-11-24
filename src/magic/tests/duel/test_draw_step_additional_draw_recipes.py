"""Focused checks for the actual Rites of Flourishing and Well of Ideas riders."""

from copy import deepcopy
import unittest

from draw_step_additional_draw_recipes import (
    FAMILY, RitesDrawCase, WellDrawCase, classify, profiles, spec, variants,
)


class AdditionalDrawTests(unittest.TestCase):
    def setUp(self):
        pins = profiles()['sources'] | profiles()['witnesses']
        self.rows = {name: row['registry'] for name, row in pins.items()}
        self.oracle = {name: row['oracle'] for name, row in pins.items()}

    def build(self, model, name, mode, player):
        board = model(self.oracle[name], self.rows[name]['csvid'], player, mode, self.rows, self.oracle)
        board.build()
        return board

    def test_symmetric_rites_allowance_includes_same_turn_third_land_denial(self):
        for owner in (0, 1):
            for mode, player in (('owner_quota', owner), ('opposing_quota', 1 - owner)):
                board = self.build(RitesDrawCase, 'Rites of Flourishing', mode, owner)
                self.assertEqual(board.land_player, player)
                self.assertEqual([s['value'] for s in board.steps if s['op'] == 'play_land'], [1, 1, 0])
                self.assertEqual(dict(board.draws), {0: 2, 1: 2})
        board = self.build(RitesDrawCase, 'Rites of Flourishing', 'stacked_quota', 0)
        self.assertEqual([s['value'] for s in board.steps if s['op'] == 'play_land'], [1, 1, 1, 0])
        self.assertEqual(dict(board.draws), {0: 3, 1: 3})

    def test_well_entry_recipient_is_separate_from_each_draw_step_amount(self):
        for owner in (0, 1):
            board = self.build(WellDrawCase, 'Well of Ideas', 'asymmetric', owner)
            self.assertEqual(board.draws, [(1 - owner, 2), (owner, 3)])
            self.assertEqual([len(hand) for hand in board.hand], [5 if seat == owner else 2 for seat in (0, 1)])
            countered = self.build(WellDrawCase, 'Well of Ideas', 'countered', owner)
            self.assertEqual([len(hand) for hand in countered.hand], [1, 1])
            self.assertIn('cast_response', [s['op'] for s in countered.steps])

    def test_whole_source_and_exact_declared_cost_are_checked(self):
        record = self.oracle['Rites of Flourishing']
        model = classify(record, 9004)
        declaration = dict(family=FAMILY, version=2, params=model, oracle_text=record['oracle_text'],
                           variants=variants(model))
        self.assertEqual(spec(declaration, record)['csvid'], 9004)
        wrong_cost = deepcopy(declaration)
        wrong_cost['params']['price'][0] -= 1
        with self.assertRaises(ValueError):
            spec(wrong_cost, record)
        with self.assertRaises(ValueError):
            spec(declaration, dict(record, oracle_text=record['oracle_text'].splitlines()[0]))
