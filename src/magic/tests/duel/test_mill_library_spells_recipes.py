import copy
import unittest

import mill_library_spells_recipes as recipe


class MillLibrarySpellsTests(unittest.TestCase):
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

    def test_flashback_mills_twice_the_same_declared_x_and_exiles_source(self):
        for owner in (0, 1):
            ordinary = self.play(recipe.CONFUSION, 'normal_x2', owner)
            flashback = self.play(recipe.CONFUSION, 'flashback_x2', owner)
            self.assertEqual(len(ordinary.milled), 2)
            self.assertEqual(len(flashback.milled), 4)
            self.assertEqual(flashback.milled, list(recipe.FOE_DECK[:4]))
            self.assertEqual([s['value'] for s in flashback.steps if s['op'] == 'choose_x'], [2])
            self.assertEqual(ordinary.zones[owner]['graveyard'][-1], self.sources[recipe.CONFUSION])
            self.assertEqual(flashback.zones[owner]['exile'], [self.sources[recipe.CONFUSION]])

    def test_traumatize_uses_current_length_after_a_paid_response(self):
        for owner in (0, 1):
            game = self.play(recipe.TRAUMATIZE, 'live_draw', owner)
            self.assertEqual(game.zones[1-owner]['hand'], [recipe.FOE_DECK[0]])
            self.assertEqual(game.milled, list(recipe.FOE_DECK[1:3]))
            self.assertEqual(game.zones[1-owner]['library'], list(recipe.FOE_DECK[3:6]))

    def test_modal_mill_clamps_each_library_independently(self):
        for owner in (0, 1):
            game = self.play(recipe.FASCINATION, 'mill_x3_short', owner)
            self.assertEqual(game.zones[owner]['library'], list(recipe.CASTER_DECK[3:]))
            self.assertEqual(game.zones[1-owner]['library'], [])
            self.assertEqual(game.milled, list(recipe.CASTER_DECK[:3]) + [recipe.FOE_DECK[0]])
            drawn = self.play(recipe.FASCINATION, 'draw_x2', owner)
            self.assertEqual(drawn.zones[owner]['hand'], list(recipe.CASTER_DECK[:2]))
            self.assertEqual(drawn.zones[1-owner]['hand'], list(recipe.FOE_DECK[:2]))

    def test_dread_summons_counts_only_newly_milled_creatures(self):
        for owner in (0, 1):
            mixed = self.play(recipe.SUMMONS, 'mixed_x4', owner)
            self.assertEqual(mixed.zombies, 3)
            self.assertIn(dict(op='assert_matching_permanents', player=owner,
                               index=self.rows[recipe.ZOMBIE]['csvid'], property='tapped', value=1), mixed.steps)
            empty = self.play(recipe.SUMMONS, 'no_creatures', owner)
            self.assertEqual(empty.zombies, 0)
            self.assertEqual(len(empty.milled), 6)
        changed = copy.deepcopy(self.oracle[self.sources[recipe.SUMMONS]])
        changed['oracle_text'] = changed['oracle_text'].replace('tapped 2/2', '2/2')
        with self.assertRaises(ValueError):
            recipe.classify(changed, recipe.SUMMONS)
