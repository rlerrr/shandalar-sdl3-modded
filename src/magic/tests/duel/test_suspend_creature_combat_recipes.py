"""Focused controls on actual Suspend choices and the Shade repair."""

from copy import deepcopy
import unittest

from suspend_creature_combat_recipes import CombatSuspend, FAMILY, classify, profiles, spec, variants


class SuspendCreatureCombatTests(unittest.TestCase):
    def setUp(self):
        pins = profiles()['sources'] | profiles()['witnesses']
        self.oracle = {n: pin['oracle'] for n, pin in pins.items()}
        self.rows = {n: pin['registry'] for n, pin in pins.items()}

    def board(self, name, mode, actor=0):
        game = CombatSuspend(self.rows[name]['csvid'], self.oracle[name], actor, mode, self.rows, self.oracle)
        game.build()
        return game

    def test_equal_riftmarked_costs_offer_explicit_suspend_and_normal_rows(self):
        for mode, value in [('parked', 1), ('normal', 0)]:
            game = self.board('Riftmarked Knight', mode)
            offered, = (s['value'] for s in game.steps if s['op'] == 'choose_dialog')
            self.assertEqual(offered, value)
            self.assertNotIn('knight', game.cards)

    def test_shade_observes_white_fee_before_clearing_for_untap(self):
        game = self.board('Shade of Trokair', 'countdown')
        steps = game.steps
        activation, = (i for i, entry in enumerate(steps) if entry['op'] == 'activate')
        paid = steps.index(dict(op='assert_mana', player=0, color=5, value=0), activation)
        attack = steps.index(dict(op='attack', ref='played', value=1), paid)
        clear = steps.index(dict(op='set_mana', player=0, color=5, value=0), attack)
        self.assertEqual(steps[clear + 1:clear + 3], [dict(op='refresh'), dict(op='advance_turn')])
        self.assertEqual(steps[activation]['value'], 1)
        self.assertEqual(game.life[1], 18)

    def test_exact_suspend_price_guard(self):
        record = self.oracle['Ith, High Arcanist']
        model = classify(record, 8582)
        entry = dict(family=FAMILY, version=2, params=model, oracle_text=record['oracle_text'],
                     variants=variants(model))
        self.assertEqual(spec(entry, record)['fee'], [0, 0, 1, 0, 0, 1])
        mutation = deepcopy(entry)
        mutation['params']['fee'][3] = 1
        with self.assertRaises(ValueError):
            spec(mutation, record)
