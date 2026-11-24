"""Guard Threshold model and payment/real zone timing."""

from copy import deepcopy
import unittest

from threshold_combat_activated_recipes import ActivatedThresholdDuel, FAMILY, classify, profiles, spec, variants


class ThresholdActivatedTests(unittest.TestCase):
    def setUp(self):
        pins = profiles()['sources'] | profiles()['witnesses']
        self.rows = {n: pin['registry'] for n, pin in pins.items()}
        self.oracle = {n: pin['oracle'] for n, pin in pins.items()}

    def board(self, name, seat, mode):
        game = ActivatedThresholdDuel(self.rows[name]['csvid'], self.oracle[name], seat, mode, self.rows, self.oracle)
        game.build()
        return game

    def test_wrong_color_leaves_the_actual_discard_witness_in_hand(self):
        game = self.board('Dirty Wererat', 1, 'regen_wrong_mana')
        self.assertEqual(game.cards['discard']['zone'], 'hand')
        self.assertTrue(game.active)
        self.assertIn(dict(op='assert_mana', player=1, color=4, value=1), game.steps)
        self.assertIn(dict(op='assert_unused_choices', value=1), game.steps)

    def test_replay_loss_exiles_seven_mountains_and_accounts_for_real_crypt_sacrifice(self):
        for seat, size in ((0, 8), (1, 7)):
            game = self.board('Fledgling Dragon', seat, 'replay')
            suffix = game.steps[game.snapshot:]
            self.assertIn(dict(op='activate', ref='crypt', value=1), suffix)
            self.assertIn(dict(op='assert_zone', player=seat, zone='exile', value=size), suffix)
            self.assertFalse(game.active)

    def test_exact_source_pin_rejects_altered_threshold_modifier(self):
        record = self.oracle['Putrid Imp']
        model = classify(record, 5866)
        entry = dict(family=FAMILY, version=2, oracle_text=record['oracle_text'],
                     params=model, variants=variants(model))
        self.assertEqual(spec(entry, record)['delta'], 1)
        changed = deepcopy(entry)
        changed['params']['delta'] = 0
        with self.assertRaises(ValueError):
            spec(changed, record)
