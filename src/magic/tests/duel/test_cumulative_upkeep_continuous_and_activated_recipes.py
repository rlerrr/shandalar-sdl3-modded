"""Guard the payable upkeep and distinct continuous/activation branch fixtures."""

import unittest

import cumulative_upkeep_continuous_and_activated_recipes as model


class ContinuousUpkeepGuard(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        pins = model.profiles()
        by_name = pins['sources'] | pins['witnesses']
        cls.oracle = {name: entry['oracle'] for name, entry in by_name.items()}
        cls.rows = {name: entry['registry'] for name, entry in by_name.items()}
        cls.names = {row['csvid']: name for name, row in cls.rows.items()}

    def game(self, csvid, seat, mode):
        game = model.Duel(csvid, self.oracle[self.names[csvid]], seat, mode, self.rows, self.oracle)
        game.build()
        return game

    def test_rejects_partial_oracle_and_unreviewed_variant(self):
        record = self.oracle[self.names[model.CORROSION]]
        shape = model.classify(record, model.CORROSION)
        declaration = dict(family=model.FAMILY, version=2, params=shape,
                           oracle_text=record['oracle_text'], variants=['rust_expiry_1'])
        self.assertEqual(model.spec(declaration, record)['upkeep'], [1, 0, 0, 0, 0, 0])
        with self.assertRaises(ValueError):
            model.spec(declaration, {**record, 'oracle_text': record['oracle_text'].rsplit('\n', 1)[0]})
        with self.assertRaises(ValueError):
            model.spec({**declaration, 'variants': ['velocity_1']}, record)

    def test_rust_threshold_owner_and_leave_reset_are_distinct(self):
        for seat in (0, 1):
            game = self.game(model.CORROSION, seat, 'rust_expiry')
            self.assertEqual([s['ref'] for s in game.steps if s['op'] == 'cast'],
                             ['source', 'remove', 'replacement'])
            self.assertEqual(game.cards['artifact_one']['zone'], 'graveyard')
            self.assertEqual(game.cards['artifact_two']['zone'], 'graveyard')
            for key in ('own_artifact', 'artifact_three', 'foe_creature'):
                self.assertEqual(game.cards[key]['zone'], 'battlefield')
            self.assertEqual(game.rust_round, 1)

    def test_music_fee_belongs_to_target_controller_after_source_leaves(self):
        for seat in (0, 1):
            game = self.game(model.MUSICIAN, seat, 'music_paid_after_source_removed')
            self.assertEqual(game.cards['source']['zone'], 'graveyard')
            self.assertEqual(game.cards['subject']['zone'], 'battlefield')
            intents = [s for s in game.steps if s['op'] == 'choose_dialog']
            self.assertEqual((intents[-1]['player'], intents[-1]['value']), (1 - seat, 0))
            self.assertTrue(any(s['op'] == 'assert_card' and s['ref'] == 'foe_pay_land'
                                and s['property'] == 'tapped' and s['value'] == 1 for s in game.steps))

    def test_terrain_cast_choices_and_later_mana_probes(self):
        for seat in (0, 1):
            for mode in ('terrain_conversion', 'terrain_expiry', 'terrain_identity'):
                game = self.game(model.TERRAIN, seat, mode)
                choices = [s for s in game.steps if s['op'] == 'choose_color']
                self.assertEqual([(s['player'], s['value']) for s in choices],
                                 [(seat, 3), (seat, 3 if mode == 'terrain_identity' else 4)])
                self.assertEqual(game.live_source(), mode != 'terrain_expiry')
                producers = [s['ref'] for s in game.steps if s['op'] == 'activate']
                self.assertEqual(producers, [f'forest_{seat}', f'island_{seat}',
                                             f'forest_{1-seat}', f'island_{1-seat}', 'nonbasic_forest'])


if __name__ == '__main__':
    unittest.main()
