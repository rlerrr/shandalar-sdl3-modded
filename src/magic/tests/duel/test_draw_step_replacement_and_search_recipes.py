"""Focused guards for draw replacement and separate printed effects."""

from copy import deepcopy
import unittest

import draw_step_replacement_and_search_recipes as model


class DrawStepReplacementAndSearchTests(unittest.TestCase):
    def setUp(self):
        self.pins = model.profiles()
        self.oracle = {n: pin['oracle'] for group in self.pins.values() for n, pin in group.items()}
        self.rows = {n: pin['registry'] for group in self.pins.values() for n, pin in group.items()}

    def game(self, name, mode, seat=0):
        game = model.AddedGame(self.rows[name], seat, mode, self.oracle, self.rows)
        game.build()
        return game

    def test_exact_oracle_record_and_variant_guard(self):
        for pin in self.pins['sources'].values():
            record = pin['oracle']
            csvid = pin['registry']['csvid']
            shape = model.classify(record, csvid)
            declaration = dict(family=model.FAMILY, version=2, oracle_text=record['oracle_text'],
                               params=shape, variants=model.variants(shape))
            self.assertEqual(model.spec(declaration, record)['csvid'], csvid)
            bad = deepcopy(declaration)
            bad['params']['mana'][0] += 1
            with self.assertRaises(ValueError):
                model.spec(bad, record)
            with self.assertRaises(ValueError):
                model.spec(declaration, dict(record, oracle_text=record['oracle_text'] + '.'))

    def test_life_payment_real_stack_and_next_own_end(self):
        for seat in (0, 1):
            game = self.game('Necropotence', 'life_other_turn', seat)
            ops = [step['op'] for step in game.steps]
            activated = ops.index('activate')
            cleanup = ops.index('cleanup')
            self.assertLess(activated, cleanup)
            self.assertEqual(ops[activated + 1:activated + 4], ['assert_stack', 'assert_life', 'resolve'])
            self.assertEqual(game.life[seat], 19)
            self.assertEqual([step['player'] for step in game.steps if step['op'] == 'draw_step'], [1 - seat, seat])
            self.assertEqual(game.cards[f'lib_{seat}_0']['zone'], 'hand')
            self.assertTrue(all(step['value'] == 0 for step in game.steps[activated:cleanup]
                                if step['op'] == 'assert_zone' and step.get('zone') == 'exile'))

    def test_optional_opponent_draw_and_two_outside_step_draws(self):
        passed = self.game('Psychic Possession', 'decline_optional')
        accepted = self.game('Psychic Possession', 'spell_draws')
        self.assertEqual(len(passed.libraries[0]), 5)
        self.assertEqual(len(accepted.libraries[0]), 2)
        self.assertEqual(sum(op['op'] == 'choose_trigger_source' for op in accepted.steps), 3)
        self.assertEqual(accepted.cards['foe_spell']['zone'], 'graveyard')

    def test_taigam_exact_fees_and_real_graveyard_exile(self):
        for count, mode in ((1, 'shrink_one'), (2, 'shrink_two')):
            game = self.game("Taigam, Sidisi's Hand", mode)
            self.assertEqual(sum(c['zone'] == 'exile' for c in game.cards.values()), count)
            self.assertEqual([r['value'] for r in game.steps if r.get('ref') == 'victim'
                              and r.get('property') == 'toughness'], [3 - count, 3])
            self.assertEqual(sum(op['op'] == 'activate' for op in game.steps), 1)
            self.assertEqual([op for op in game.steps if op['op'] == 'draw_step'],
                             [dict(op='draw_step', player=1), dict(op='draw_step', player=0)])
        empty = self.game("Taigam, Sidisi's Hand", 'zero_eligible')
        self.assertFalse(any(c['zone'] == 'graveyard' for c in empty.cards.values()))
        self.assertTrue(any(op['op'] == 'can_activate' and op['value'] == 1 for op in empty.steps))
