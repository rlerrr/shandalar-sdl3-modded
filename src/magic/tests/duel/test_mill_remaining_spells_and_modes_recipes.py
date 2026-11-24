import copy
import unittest

import mill_remaining_spells_and_modes_recipes as recipe


class MillSpellsAndModesTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        pins = recipe.profiles()
        cls.oracle = {name: pin['oracle'] for group in pins.values() for name, pin in group.items()}
        cls.rows = {name: pin['registry'] for group in pins.values() for name, pin in group.items()}
        cls.sources = {cls.rows[name]['csvid']: name for name in pins['sources']}

    def play(self, csvid, mode, owner):
        game = recipe.FollowerCase(self.oracle[self.sources[csvid]], csvid, owner, mode, self.rows, self.oracle)
        game.play()
        return game

    def test_join_forces_uses_paid_contributions_in_caster_first_order(self):
        for owner in (0, 1):
            foe = 1-owner
            game = self.play(recipe.TRAUMA, 'split', owner)
            self.assertEqual([(a['player'], a['value']) for a in game.steps if a['op'] == 'choose_x'],
                             [(owner, 2), (foe, 1)])
            ops = [a['op'] for a in game.steps]
            self.assertLess(ops.index('cast'), ops.index('choose_x'))
            self.assertLess(ops.index('choose_x'), ops.index('resolve'))
            self.assertEqual(game.pools[owner], (2, 0, 0, 0, 0, 0))
            self.assertEqual(game.pools[foe], (2, 0, 0, 0, 0, 0))
            self.assertEqual(game.milled, list(recipe.OWN_MIX[:3]) + list(recipe.LIBRARY[:3]))

    def test_join_forces_keeps_zero_contributions_and_clamps_each_library(self):
        game = self.play(recipe.TRAUMA, 'decline', 0)
        self.assertEqual(game.milled, [])
        self.assertEqual([pool[0] for pool in game.pools], [1, 2])
        short = self.play(recipe.TRAUMA, 'short_split', 1)
        self.assertEqual(len(short.milled), 7)
        self.assertEqual(short.zones[0]['library'], [])
        self.assertEqual(short.zones[1]['library'], list(recipe.OWN_MIX[5:]))

    def test_indignation_losses_use_only_new_milled_creatures_despite_prevention(self):
        for owner in (0, 1):
            foe = 1-owner
            typed = self.play(recipe.INDIGNATION, 'through_purity', owner)
            self.assertEqual(typed.life[foe], 18)
            self.assertIn(dict(op='assert_life', player=foe, value=22), typed.steps)
            self.assertEqual(typed.life[owner], 20)
            self.assertEqual(typed.zones[owner]['library'], list(recipe.OWN_MIX[4:]))
            self.assertEqual(typed.zones[foe]['library'], list(recipe.LIBRARY))
            no_creatures = self.play(recipe.INDIGNATION, 'no_creatures', owner)
            self.assertEqual(no_creatures.life, [20, 20])

    def test_pin_rejects_damage_as_a_substitute_for_life_loss(self):
        record = copy.deepcopy(self.oracle[self.sources[recipe.INDIGNATION]])
        record['oracle_text'] = record['oracle_text'].replace('loses 2 life', 'is dealt 2 damage')
        with self.assertRaises(ValueError):
            recipe.classify(record, recipe.INDIGNATION)
