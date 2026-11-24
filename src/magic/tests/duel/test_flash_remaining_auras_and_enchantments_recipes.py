import copy
import unittest

import flash_remaining_auras_and_enchantments_recipes as recipe


class FlashEnchantmentsTests(unittest.TestCase):
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

    def test_maggot_zero_toughness_causes_both_graves(self):
        for owner in (0, 1):
            game = self.play(recipe.MAGGOT, 'lethal_attack', owner)
            self.assertEqual(game.cards['attacker']['zone'], 'graveyard')
            self.assertEqual(game.cards['source']['zone'], 'graveyard')
            self.assertEqual(game.life, [20, 20])
            ops = [step['op'] for step in game.steps]
            self.assertLess(ops.index('finish_attackers'), ops.index('cast'))
            self.assertLess(ops.index('resolve'), ops.index('combat_damage'))

    def test_vehicle_remains_a_legal_aura_host_before_crew(self):
        for owner in (0, 1):
            game = self.play(recipe.MELTDOWN, 'vehicle_energy', owner)
            self.assertEqual(game.stats('host'), (-2, 3))
            self.assertTrue(game.cards['pilot']['tapped'])
            self.assertEqual(game.energy, 2)
            ops = [step['op'] for step in game.steps]
            self.assertLess(ops.index('resolve_response'), ops.index('activate'))
            failed = self.play(recipe.MELTDOWN, 'lost_target', owner)
            self.assertEqual(failed.energy, 0)
            self.assertEqual(failed.cards['source']['zone'], 'graveyard')

    def test_howlpack_buff_and_trample_follow_control(self):
        for owner in (0, 1):
            scope = self.play(recipe.HOWLPACK, 'team_scope', owner)
            self.assertEqual([scope.stats(ref) for ref in ('wolf', 'werewolf', 'own_other')],
                             [(3, 3), (3, 2), (1, 1)])
            self.assertEqual([scope.stats(ref) for ref in ('foe_wolf', 'foe_werewolf')], [(2, 2), (2, 1)])
            trample = self.play(recipe.HOWLPACK, 'team_trample', owner)
            removed = self.play(recipe.HOWLPACK, 'team_remove', owner)
            self.assertEqual((trample.life[1-owner], removed.life[1-owner]), (18, 20))

    def test_dictate_is_symmetric_and_expires_on_removal(self):
        for owner in (0, 1):
            game = self.play(recipe.DICTATE, 'direct_damage', owner)
            self.assertEqual(game.cards['durable']['damage'], 4)
            self.assertEqual(game.life[1-owner], 16)
            removed = self.play(recipe.DICTATE, 'remove_before_damage', owner)
            self.assertEqual(removed.cards['durable']['damage'], 2)
        record = copy.deepcopy(self.oracle[self.sources[recipe.DICTATE]])
        record['oracle_text'] = record['oracle_text'].replace('a permanent or player', 'an opponent')
        with self.assertRaises(ValueError):
            recipe.classify(record, recipe.DICTATE)
