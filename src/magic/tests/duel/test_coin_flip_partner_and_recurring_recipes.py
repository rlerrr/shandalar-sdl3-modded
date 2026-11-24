"""Checks for the distinct Partner, coin and loyalty branches."""

from copy import deepcopy
import unittest

import coin_flip_partner_and_recurring_recipes as model
from next_upkeep_cantrip_additional_riders_recipes import xorshift


class CoinFlipPartnerAndRecurringTests(unittest.TestCase):
    def setUp(self):
        self.pins = model.profiles()
        self.oracle = {n: pin['oracle'] for group in self.pins.values() for n, pin in group.items()}
        self.rows = {n: pin['registry'] for group in self.pins.values() for n, pin in group.items()}

    def game(self, name, mode, owner):
        game = model.Game(self.rows[name], owner, mode, self.rows, self.oracle)
        game.build()
        return game

    def test_whole_oracle_and_actual_cost_required(self):
        for pin in self.pins['sources'].values():
            oracle = pin['oracle']
            shape = model.classify(oracle, pin['registry']['csvid'])
            declaration = dict(family=model.FAMILY, version=2, params=shape, oracle_text=oracle['oracle_text'],
                               variants=model.variants(shape))
            self.assertEqual(model.spec(declaration, oracle)['csvid'], shape['csvid'])
            bad = deepcopy(declaration)
            bad['params']['mana'][0] += 1
            with self.assertRaises(ValueError):
                model.spec(bad, oracle)
            with self.assertRaises(ValueError):
                model.spec(declaration, dict(oracle, oracle_text=oracle['oracle_text'] + '.'))

    def test_real_ral_seed_windows_for_both_call_paths(self):
        for name, (seed, expected, heads) in model.RAL_SEEDS.items():
            state = seed
            bits = []
            for _ in expected:
                state = xorshift(state)
                bits.append(state & 1)
            self.assertEqual(tuple(bits), expected)
            for stride in (1, 2):
                for start in range(len(bits) - 4 * stride):
                    self.assertEqual(sum(bits[start + i * stride] == 0 for i in range(5)), heads)
            for actor in (0, 1):
                game = self.game('Ral Zarek', name, actor)
                self.assertEqual([op['value'] for op in game.steps if op['op'] == 'advance_scheduled_turn'],
                                 [actor] * heads + [1 - actor, actor])

    def test_actual_search_decline_and_bilateral_coin_payoff(self):
        name = 'Zndrsplt, Eye of Wisdom'
        for actor in (0, 1):
            fail = self.game(name, 'fail_to_find', actor)
            self.assertEqual(fail.library[actor], ['Okaun, Eye of Chaos', 'Forest'])
            self.assertEqual(fail.shuffles[actor], 1)
            accepted = self.game(name, 'own_flip_win', actor)
            self.assertFalse(accepted.library[actor])
            self.assertEqual(accepted.cards['coin_witness']['player'], actor)
            self.assertEqual([op['player'] for op in accepted.steps if op['op'] == 'upkeep_step'], [1 - actor, actor])

    def test_target_loss_and_countered_provider_use_real_stack(self):
        for actor in (0, 1):
            ral = self.game('Ral Zarek', 'first_target_lost', actor)
            self.assertEqual(ral.cards['victim']['zone'], 'hand')
            self.assertEqual(ral.tapped['owner_land'], 0)
            self.assertEqual(ral.loyalty['source'], 5)
            self.assertTrue(any(op['op'] == 'assert_stack' and op['value'] == 2 for op in ral.steps))
            sliver = self.game('Frenetic Sliver', 'countered', actor)
            self.assertEqual(sliver.cards['source']['zone'], 'graveyard')
            self.assertEqual(sliver.cards['counter']['zone'], 'graveyard')
