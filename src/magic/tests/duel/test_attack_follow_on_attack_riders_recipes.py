"""Guards for the boundaries that distinguish the attack rider variants."""
from copy import deepcopy
import unittest

import attack_follow_on_attack_riders_recipes as model


class AttackRiderTests(unittest.TestCase):
    def setUp(self):
        self.pins = model.profiles()
        entries = self.pins['sources'] | self.pins['witnesses']
        self.oracle = {name: p['oracle'] for name, p in entries.items()}
        self.rows = {name: p['registry'] for name, p in entries.items()} | self.pins['engine_faces']
        self.ids = {row['csvid']: row for row in self.rows.values()}

    def board(self, key, mode, seat):
        board = model.FollowBoard(self.ids[key], mode, seat, self.rows, self.oracle)
        board.run()
        return board

    def test_exact_oracle_and_physical_animator_pins(self):
        model.validate_witnesses(self.oracle, self.rows)
        altered = deepcopy(self.oracle['Raging Ravine'])
        altered['oracle_text'] += ' Untap it.'
        with self.assertRaises(ValueError):
            model.classify(altered, model.RAVINE)
        rows = self.rows | {model.ANIMATED: self.rows[model.ANIMATED] | {'iid': 0}}
        with self.assertRaises(ValueError):
            model.validate_witnesses(self.oracle, rows)

    def test_ravine_counters_cross_real_expiry_and_reanimation(self):
        for seat in (0, 1):
            game = self.board(model.RAVINE, 'attack_twice', seat)
            self.assertEqual([step['value'] for step in game.steps if step['op'] == 'assert_counter'], [0, 1, 1, 1, 2])
            self.assertEqual(sum(step['op'] == 'play_land' and step['value'] == 1 for step in game.steps), 1)
            self.assertIn(dict(op='assert_card',ref='source',property='csvid',value=model.RAVINE), game.steps)

    def test_grabber_rebinds_real_recipient_and_return(self):
        for seat in (0, 1):
            steps = self.board(model.OGRE, 'attached', seat).steps
            self.assertEqual([(s['ref'], s['player']) for s in steps if s['op'] == 'bind_permanent'],
                             [('stolen', seat), ('returned', 1-seat)])
            self.assertEqual([s['value'] for s in steps if s['op'] == 'assert_card' and s['ref'] == 'enemy'
                              and s['property'] == 'power'], [4, 2, 2])

    def test_join_forces_keeps_oracle_both_seat_payment_order(self):
        for seat in (0, 1):
            for mode in ('attack_paid', 'block_paid'):
                steps = self.board(model.MANA_DRAGON, mode, seat).steps
                self.assertEqual([(s['player'], s['value']) for s in steps if s['op'] == 'choose_x'],
                                 [(seat, 2), (1-seat, 1)])

    def test_dash_return_and_ordinary_nonreturn_are_distinct(self):
        for seat in (0, 1):
            dashed = self.board(model.VAULT, 'dash_loot', seat)
            normal = self.board(model.VAULT, 'normal_loot', seat)
            self.assertEqual((dashed.cards['source'][2], normal.cards['source'][2]), (model.HAND, model.BATTLE))
            self.assertEqual([s['value'] for s in normal.steps if s['op'] == 'choose_dialog'], [1])
