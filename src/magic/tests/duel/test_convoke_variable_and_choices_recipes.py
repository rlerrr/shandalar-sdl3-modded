"""Focused guards for live variable Convoke and its fixture repair."""

from copy import deepcopy
import unittest

import convoke_variable_and_choices_recipes as model


class VariableConvokeTests(unittest.TestCase):
    def scene(self, csvid, seat, mode):
        pins = model.pins()
        all_pins = pins['sources'] | pins['witnesses']
        rows = {name: data['registry'] for name, data in all_pins.items()}
        oracle = {name: data['oracle'] for name, data in all_pins.items()}
        row, = [entry for entry in rows.values() if entry['csvid'] == csvid]
        scene = model.Scene(row, seat, mode, rows, oracle)
        scene.play()
        return scene

    def test_full_oracle_pin(self):
        for record in model.pins()['sources'].values():
            oracle, csvid = record['oracle'], record['registry']['csvid']
            self.assertEqual(model.classify(oracle, csvid)['csvid'], csvid)
            changed = deepcopy(oracle)
            changed['oracle_text'] += ' '
            with self.assertRaises(ValueError):
                model.classify(changed, csvid)

    def test_ranks_human_payment_brackets_grave_targets(self):
        scene = self.scene(model.RANKS, 0, 'two_taps')
        inputs = [s for s in scene.steps if s['op'].startswith('choose_')]
        self.assertEqual([s['op'] for s in inputs],
                         ['choose_target'] * 2 + ['choose_zone_card'] * 2 + ['choose_target'] * 2)
        self.assertEqual([s['value'] for s in inputs if s['op'] == 'choose_zone_card'], [106, 514])

    def test_ai_ranks_uses_actual_grave_selection_without_x_menu(self):
        scene = self.scene(model.RANKS, 1, 'mixed_x')
        self.assertEqual([s['op'] for s in scene.steps if s['op'].startswith('choose_')], ['choose_zone_card'] * 2)
        self.assertEqual(scene.tapped, {'helper_0', 'helper_1', 'helper_2'})
        zero = self.scene(model.RANKS, 1, 'zero')
        self.assertIn('Return to the Ranks', zero.zones[1]['graveyard'])
        self.assertEqual(zero.tapped, {'helper_0', 'helper_1'})

    def test_sprout_buyback_and_decline_have_independent_payments(self):
        for seat in (0, 1):
            buy = self.scene(model.SPROUT, seat, 'buyback')
            decline = self.scene(model.SPROUT, seat, 'decline')
            self.assertEqual((len(buy.tapped), len(decline.tapped)), (5, 2))
            self.assertIn('Sprout Swarm', buy.zones[seat]['hand'])
            self.assertIn('Sprout Swarm', decline.zones[seat]['graveyard'])

    def test_stain_real_zones_and_human_scope(self):
        exile = self.scene(model.STAIN, 1, 'exile')
        self.assertEqual(exile.zones[0]['exile'], ['Hill Giant'] * 3)
        self.assertEqual(exile.shuffle, [1, 0])
        shape = {'csvid': model.STAIN}
        self.assertIn('countered_0', model.variants(shape))
        self.assertNotIn('exile_0', model.variants(shape))

    def test_march_retains_real_x_and_lifelink(self):
        for seat in (0, 1):
            mixed = self.scene(model.MARCH, seat, 'mixed_x')
            self.assertEqual([s['value'] for s in mixed.steps if s['op'] == 'choose_x'], [0, 1])
            self.assertEqual(mixed.zones[seat]['battlefield'].count('Soldier'), 2)
            combat = self.scene(model.MARCH, seat, 'lifelink')
            self.assertIn(dict(op='attack', ref='soldier', value=1), combat.steps)
            self.assertEqual((combat.life[seat], combat.life[1 - seat]), (21, 19))
