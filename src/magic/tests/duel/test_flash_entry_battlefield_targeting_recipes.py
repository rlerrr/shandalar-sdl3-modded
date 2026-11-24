import copy
import unittest

import flash_entry_battlefield_targeting_recipes as recipe


class FlashBattlefieldTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        pins = recipe.profiles()
        cls.oracle = {name: pin['oracle'] for group in pins.values() for name, pin in group.items()}
        cls.rows = {name: pin['registry'] for group in pins.values() for name, pin in group.items()}
        cls.source = {cls.rows[name]['csvid']: name for name in pins['sources']}

    def play(self, csvid, mode, owner):
        game = recipe.FlashCase(self.oracle[self.source[csvid]], csvid, owner, mode, self.rows, self.oracle)
        game.play()
        return game

    def test_restoration_rebind_searches_by_identity_after_real_reentry(self):
        for owner in (0, 1):
            game = self.play(recipe.ANGEL, 'blink_reentry', owner)
            bind, = (s for s in game.steps if s['op'] == 'bind_permanent')
            self.assertEqual(bind['index'], -1)
            self.assertEqual(bind['value'], self.rows[recipe.ENTRY_WITNESS]['csvid'])
            self.assertEqual(bind['player'], owner)
            self.assertEqual(game.life[owner], 28)
            self.assertFalse(game.cards['missionary']['tapped'])

    def test_vizier_real_combat_precedes_removal_and_owner_return(self):
        for owner in (0, 1):
            game = self.play(recipe.VIZIER, 'foreign_attacker', owner)
            ops = [s['op'] for s in game.steps]
            self.assertLess(ops.index('finish_attackers'), ops.index('cast'))
            self.assertLess(ops.index('end_combat'), ops.index('cleanup'))
            self.assertLess(ops.index('cleanup'), ops.index('bind_permanent'))
            self.assertEqual(game.cards['foe_creature']['player'], owner)
            self.assertTrue(any(s['op'] == 'assert_zone_multiplicity' and s['zone'] == 'exile'
                                and s['player'] == owner and s['value'] == 1 for s in game.steps))
            idle = self.play(recipe.VIZIER, 'idle_target', owner)
            self.assertEqual(idle.cards['idle']['zone'], 'battlefield')
            self.assertEqual(idle.life[owner], 19)

    def test_trickster_ability_restores_while_the_creature_stays_tapped(self):
        for owner in (0, 1):
            game = self.play(recipe.TRICKSTER, 'already_tapped', owner)
            queries = [s['value'] for s in game.steps if s['op'] == 'can_activate' and s['ref'] == 'foe_creature']
            self.assertEqual(queries, [1, 0, 1])
            self.assertTrue(game.cards['foe_creature']['tapped'])
            self.assertEqual(game.pools[1-owner], recipe.ZERO)

    def test_exact_printed_condition_is_pin_guarded(self):
        name = self.source[recipe.VIZIER]
        record = self.oracle[name]
        altered = copy.deepcopy(record)
        altered['oracle_text'] = altered['oracle_text'].replace('attacked or blocked', 'blocked')
        with self.assertRaises(ValueError):
            recipe.classify(altered, recipe.VIZIER)
