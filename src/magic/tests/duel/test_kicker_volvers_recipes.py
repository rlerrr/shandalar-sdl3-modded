"""Focused checks for compact payment menus and late replay."""

from copy import deepcopy
import unittest

from kicker_volvers_recipes import FAMILY, VolverBoard, classify, profiles, spec, variants


class VolverPaymentTests(unittest.TestCase):
    def setUp(self):
        pins = profiles()['sources'] | profiles()['witnesses']
        self.oracle = {n: pin['oracle'] for n, pin in pins.items()}
        self.rows = {n: pin['registry'] for n, pin in pins.items()}

    def board(self, name, mode):
        return VolverBoard(self.oracle[name], self.rows[name]['csvid'], 1, mode, self.rows, self.oracle)

    def test_menu_remaps_to_second_only_or_decline_when_fee_unpayable(self):
        wrong = self.board('Rakavolver', 'wrong_color')
        offered, reserve, menu = wrong.payment_plan(recast=False)
        self.assertEqual((offered, reserve, menu), ([1, 1, 1, 0, 0, 0], [1, 1, 0, 0, 0, 0], 0))
        short = self.board('Rakavolver', 'short_generic')
        offered, reserve, menu = short.payment_plan(recast=False)
        self.assertEqual((offered, reserve, menu), ([0, 0, 0, 0, 0, 1], [0, 0, 0, 0, 0, 1], 0))
        self.assertEqual((wrong.counters, wrong.traits, short.counters, short.traits),
                         (1, {'flying'}, 0, set()))

    def test_bounce_recast_checks_both_then_zero_counters(self):
        board = self.board('Rakavolver', 'bounce_recast')
        board.assemble()
        steps = board.steps
        counters = [s['value'] for s in steps if s.get('property') == 'plus1_counters']
        bind, = (i for i, s in enumerate(steps) if s['op'] == 'bind_hand')
        self.assertEqual(counters, [3, 0])
        self.assertIn({'op': 'cast', 'ref': 'reply', 'value': 1}, steps[:bind])
        self.assertIn({'op': 'cast', 'ref': 'source', 'value': 1}, steps[bind:])

    def test_exact_profiles_reject_mutated_second_kicker(self):
        record = self.oracle['Anavolver']
        model = classify(record)
        entry = dict(family=FAMILY, version=2, oracle_text=record['oracle_text'],
                     params=model, variants=variants(model))
        self.assertEqual(spec(entry, record)['family'], FAMILY)
        mutation = deepcopy(entry)
        mutation['params']['kickers'][1]['cost'][0] += 1
        with self.assertRaises(ValueError):
            spec(mutation, record)
