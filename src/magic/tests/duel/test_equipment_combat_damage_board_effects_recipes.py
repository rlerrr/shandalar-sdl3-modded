"""Check exact Worldslayer clauses and real payment/damage scenarios."""

import unittest

import equipment_combat_damage_board_effects_recipes as model


class WorldslayerGuard(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        pins = model.profiles()
        merged = pins['sources'] | pins['witnesses']
        cls.oracle = {name: pin['oracle'] for name, pin in merged.items()}
        cls.rows = {name: pin['registry'] for name, pin in merged.items()}
        cls.record = cls.oracle[model.WORLD_NAME]

    def game(self, mode, seat):
        board = model.Duel(model.WORLD, self.record, seat, mode, self.rows, self.oracle)
        board.build()
        return board

    def test_whole_rule_and_printed_equip_cost_are_required(self):
        shape = model.classify(self.record, model.WORLD)
        decl = dict(family=model.FAMILY, version=2, params=shape,
                    oracle_text=self.record['oracle_text'], variants=['hit_0'])
        self.assertEqual(model.spec(decl, self.record)['equip'], 5)
        with self.assertRaises(ValueError):
            model.spec(decl, {**self.record, 'oracle_text': self.record['oracle_text'].replace('all permanents', 'all creatures')})
        with self.assertRaises(ValueError):
            model.spec({**decl, 'params': {**shape, 'equip': 4}}, self.record)
        with self.assertRaises(ValueError):
            model.spec({**decl, 'version': 2.0}, self.record)

    def test_paid_five_mana_equip_precedes_real_player_combat_damage(self):
        for seat in (0, 1):
            game = self.game('hit', seat)
            cast = game.steps.index(dict(op='cast', ref='source', value=1))
            equip = game.steps.index(dict(op='activate', ref='source', value=1))
            attack = game.steps.index(dict(op='attack', ref='host', value=1))
            combat = game.steps.index(dict(op='combat_damage'))
            pool = dict(op='set_mana', player=seat, color=0, value=5)
            self.assertIn(pool, game.steps[:cast])
            self.assertIn(pool, game.steps[cast:equip])
            self.assertLess(equip, attack)
            self.assertLess(attack, combat)
            self.assertEqual(game.life[1 - seat], 19)

    def test_only_exact_source_copy_is_exempt(self):
        for seat in (0, 1):
            game = self.game('hit', seat)
            self.assertEqual(game.cards['other_copy']['name'], model.WORLD_NAME)
            self.assertEqual(game.cards['source']['zone'], 'battlefield')
            for ref in ('other_copy', 'host', 'own_artifact', 'foe_gear', 'own_land', 'foe_land'):
                self.assertEqual(game.cards[ref]['zone'], 'graveyard')

    def test_indestructible_survives_on_both_sides(self):
        for seat in (0, 1):
            game = self.game('indestructible', seat)
            for ref in ('own_survivor', 'foe_survivor'):
                self.assertEqual(game.cards[ref]['zone'], 'battlefield')
            self.assertEqual(game.cards['host']['zone'], 'graveyard')

    def test_block_noncombat_and_unattached_are_distinct_negative_controls(self):
        for seat in (0, 1):
            for mode in ('blocked', 'noncombat', 'unattached'):
                game = self.game(mode, seat)
                self.assertEqual(game.cards['host']['zone'], 'battlefield')
                self.assertEqual(game.cards['other_copy']['zone'], 'battlefield')
                self.assertEqual(game.life[1 - seat], 20 if mode == 'blocked' else 19)
            self.assertIn(dict(op='activate', ref='host', value=1), self.game('noncombat', seat).steps)
            self.assertNotIn(dict(op='activate', ref='source', value=1), self.game('unattached', seat).steps)
