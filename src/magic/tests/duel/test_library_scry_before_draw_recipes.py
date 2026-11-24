import unittest

import library_scry_before_draw_recipes as recipe


class ScryBeforeDrawTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        pins = recipe.profiles()
        cls.oracle = {name: pin['oracle'] for group in pins.values() for name, pin in group.items()}
        cls.rows = {name: pin['registry'] for group in pins.values() for name, pin in group.items()}
        cls.sources = {cls.rows[name]['csvid']: name for name in pins['sources']}

    def play(self, csvid, mode, owner):
        factory = recipe.FollowOn if csvid in recipe.FOLLOWERS else recipe.ScryCase
        game = factory(self.oracle[self.sources[csvid]], csvid, owner, mode, self.rows, self.oracle)
        game.play()
        return game

    def test_interpret_ok_only_when_human_reveals_a_card(self):
        game = self.play(recipe.INTERPRET, 'zero_top', 0)
        ops = [action['op'] for action in game.steps]
        self.assertEqual(game.revealed, 'Forest')
        self.assertEqual(game.drawn, [])
        self.assertEqual(ops[ops.index('resolve')-1], 'choose_dialog')
        for mode, owner in [('short', 1), ('empty', 0), ('countered', 0)]:
            self.assertNotIn('choose_dialog', [action['op'] for action in self.play(recipe.INTERPRET, mode, owner).steps])

    def test_ugin_reads_controlled_permanents_after_real_response(self):
        for owner in (0, 1):
            greatest = self.play(recipe.UGIN, 'greatest', owner)
            lost = self.play(recipe.UGIN, 'greatest_leaves', owner)
            enemy = self.play(recipe.UGIN, 'opposing_only', owner)
            self.assertEqual(len(greatest.windows[0][1]), 5)
            self.assertEqual(len(lost.windows[0][1]), 2)
            self.assertEqual(len(enemy.windows[0][1]), 0)
            self.assertIn(recipe.SERRA, lost.zones[owner]['hand'])
            self.assertEqual(len(lost.drawn), 3)

    def test_annelid_scry_windows_advance_independently(self):
        for owner in (0, 1):
            game = self.play(recipe.ANNELID, 'sequential', owner)
            self.assertEqual([len(window) for _, window in game.windows], [1, 2, 3])
            self.assertEqual(game.windows[1][1], ['Divination', 'Counterspell'])
            self.assertEqual(game.windows[2][1], ['Reach Through Mists', recipe.TINKERER, recipe.OLD_GRAVE])

    def test_empath_moves_without_draw_and_puzzleknot_pays_sacrifice(self):
        for owner in (0, 1):
            empath = self.play(recipe.EMPATH, 'creature', owner)
            self.assertEqual(empath.zones[owner]['hand'], [recipe.TINKERER])
            self.assertEqual(empath.drawn, [])
            puzzle = self.play(recipe.PUZZLE, 'paid_sacrifice', owner)
            self.assertEqual(puzzle.energy, 4)
            self.assertEqual(len(puzzle.windows), 2)
            self.assertIn(self.sources[recipe.PUZZLE], puzzle.zones[owner]['graveyard'])
            self.assertNotIn(self.sources[recipe.PUZZLE], puzzle.zones[owner]['battlefield'])
            denied = self.play(recipe.PUZZLE, 'short_payment', owner)
            self.assertEqual(denied.energy, 2)
            self.assertEqual(len(denied.windows), 1)
