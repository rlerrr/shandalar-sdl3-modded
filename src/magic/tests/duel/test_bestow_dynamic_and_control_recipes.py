"""Exact Oracle guards and independent producer/owner scenario checks."""

import unittest

import bestow_dynamic_and_control_recipes as model


class DynamicBestowGuard(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        profiles = model.profiles()
        entries = profiles['sources'] | profiles['witnesses']
        cls.oracle = {name: pin['oracle'] for name, pin in entries.items()}
        cls.rows = {name: pin['registry'] for name, pin in entries.items()}
        cls.rows.update({pin['engine_aura_face']['name']: pin['engine_aura_face'] for pin in profiles['sources'].values()})
        cls.names = {row['csvid']: name for name, row in cls.rows.items()}

    def game(self, csvid, mode, actor):
        game = model.Duel(self.rows[self.names[csvid]], actor, mode, self.rows, self.oracle)
        game.build()
        return game

    def test_exact_entire_records_and_native_aura_identity(self):
        model.validate_witnesses(self.oracle, self.rows)
        for csvid in model.CARDS:
            record = self.oracle[self.names[csvid]]
            shape = model.classify(record, csvid)
            declaration = dict(family=model.FAMILY, version=2, params=shape,
                               oracle_text=record['oracle_text'], variants=model.variants(shape))
            self.assertEqual(model.spec(declaration, record)['csvid'], csvid)
            with self.assertRaises(ValueError):
                model.spec(declaration, {**record, 'oracle_text': record['oracle_text'] + '\nDraw a card.'})
            with self.assertRaises(ValueError):
                model.spec({**declaration, 'version': True}, record)

    def test_enemy_aura_changes_host_but_does_not_count_for_eidolon(self):
        for actor in (0, 1):
            game = self.game(13689, 'enemy_aura_ignored', actor)
            self.assertEqual(game.cards['holy']['controller'], 1 - actor)
            self.assertEqual(game.holy_host, 'grown')
            self.assertEqual(game.dynamic(), 2)
            self.assertEqual([s['value'] for s in game.steps if s['op'] == 'assert_card'
                              and s['ref'] == 'source' and s['property'] == 'power'], [2, 2])

    def test_last_grave_creature_is_removed_before_nighthowler_dies(self):
        for actor in (0, 1):
            game = self.game(13490, 'last_grave_exiled', actor)
            self.assertEqual(game.cards['own_grave']['zone'], 'exile')
            self.assertEqual(game.cards['land_grave']['zone'], 'graveyard')
            self.assertEqual(game.cards['source']['zone'], 'graveyard')
            self.assertIn(('choose_player', actor), [(s['op'], s['value']) for s in game.steps])

    def test_host_dies_under_sirens_control_but_goes_to_owner(self):
        for actor in (0, 1):
            game = self.game(13912, 'stolen_dies', actor)
            self.assertEqual([(s['player'], s['ref']) for s in game.steps if s['op'] == 'bind_permanent'], [(actor, 'host')])
            self.assertEqual(game.cards['host']['zone'], 'graveyard')
            self.assertEqual(game.cards['host']['owner'], 1 - actor)
            self.assertEqual(game.cards['host']['controller'], 1 - actor)
            self.assertEqual(game.form, 'creature')

    def test_nautilus_target_comes_from_real_tap_and_nontap_abilities(self):
        for actor in (0, 1):
            for mode, tapped in (('regular_ability', 1), ('regular_nontap_ability', 0)):
                game = self.game(13861, mode, actor)
                self.assertIn(('source', 1 - actor), [(s['ref'], s['player']) for s in game.steps if s['op'] == 'choose_target'])
                self.assertEqual([s['ref'] for s in game.steps if s['op'] == 'activate'], ['pinger'])
                self.assertEqual([s['value'] for s in game.steps if s['op'] == 'assert_card'
                                  and s['ref'] == 'pinger' and s['property'] == 'tapped'], [tapped])
                self.assertEqual(game.cards['source']['zone'], 'graveyard')
