"""Ensure corrected native fixtures and exact follower riders remain linked."""
from copy import deepcopy
import unittest

import regeneration_follow_on_grants_and_modes_recipes as model


class RegenerationGrantAndModeTests(unittest.TestCase):
    def setUp(self):
        profiles = model.profiles()
        all_pins = profiles['sources'] | profiles['witnesses']
        self.oracle = {name: pin['oracle'] for name, pin in all_pins.items()}
        self.rows = {name: pin['registry'] for name, pin in all_pins.items()}
        self.by_id = {row['csvid']: row for row in self.rows.values()}

    def board(self, csvid, mode, seat):
        cls = model.FollowBoard if csvid in model.FOLLOW_MODES else model.Board
        game = cls(self.by_id[csvid], mode, seat, self.rows, self.oracle)
        game.run()
        return game

    def test_whole_pins_reject_shield_fee_changes(self):
        model.validate_witnesses(self.oracle, self.rows)
        record = deepcopy(self.oracle['Skithiryx, the Blight Dragon'])
        record['oracle_text'] = record['oracle_text'].replace('{B}{B}', '{B}')
        with self.assertRaises(ValueError):
            model.classify(record, model.DRAGON)

    def test_zone_only_grave_sources_use_exact_zone_observations(self):
        for csvid, mode in ((model.DRUDGE, 'leaves'), (model.MOLTEN, 'creature')):
            for seat in (0, 1):
                board = self.board(csvid, mode, seat)
                self.assertTrue(board.seeded_graves)
                self.assertFalse(any(s['op'] == 'assert_card' and s['ref'] in board.seeded_graves
                                     for s in board.steps))
                self.assertTrue(any(s['op'] == 'assert_zone_multiplicity' and s['zone'] == 'exile'
                                    for s in board.steps))

    def test_native_discard_modes_use_correct_kind_for_each_seat(self):
        for csvid in (model.CORE, model.MOLTEN):
            for seat in (0, 1):
                steps = self.board(csvid, 'upkeep_pay', seat).steps
                final = max(i for i, step in enumerate(steps) if step['op'] == 'upkeep_step')
                self.assertEqual(steps[final - 2], dict(op='choose_dialog', player=seat, value=0))
                discard = (dict(op='choose_zone_card', player=0, value=91) if seat == 0
                           else dict(op='choose_dialog', player=1, value=0))
                self.assertEqual(steps[final - 1], discard)

    def test_printed_inspired_and_infect_remain_actual_actions(self):
        for seat in (0, 1):
            game = self.board(model.SERVANT, 'regen_then_untap', seat)
            self.assertIn(dict(op='choose_regeneration', ref='source'), game.steps)
            self.assertIn(dict(op='untap_step', player=seat), game.steps)
            self.assertEqual(game.life[seat], 21)
            self.assertEqual(game.life[1-seat], 19)
            game = self.board(model.DRAGON, 'infect_creature', seat)
            self.assertIn(dict(op='assert_counter', ref='blocker', counter_kind='minus1', value=4), game.steps)
            self.assertIn(dict(op='assert_poison', player=1-seat, value=0), game.steps)
