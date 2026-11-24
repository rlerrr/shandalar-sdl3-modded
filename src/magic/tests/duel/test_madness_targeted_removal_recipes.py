"""Check the additional Madness timing, assignment, and shield fixtures."""

import unittest

import madness_targeted_removal_recipes as model


class MadnessRemovalGuard(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        pins = model.profiles()
        all_pins = pins['sources'] | pins['witnesses']
        cls.oracle = {name: pin['oracle'] for name, pin in all_pins.items()}
        cls.rows = {name: pin['registry'] for name, pin in all_pins.items()}
        cls.names = {row['csvid']: name for name, row in cls.rows.items()}

    def game(self, csvid, actor, mode):
        game = model.Duel(csvid, self.oracle[self.names[csvid]], actor, mode, self.rows, self.oracle)
        game.build()
        return game

    def test_full_declaration_rejects_changed_oracle_and_out_of_scope_modes(self):
        record = self.oracle[self.names[model.HUNTER]]
        shape = model.classify(record, model.HUNTER)
        declaration = dict(family=model.FAMILY, version=2, params=shape,
                           oracle_text=record['oracle_text'], variants=['shielded_normal_0', 'no_target_1'])
        self.assertEqual(model.spec(declaration, record)['madness'], shape['madness'])
        with self.assertRaises(ValueError):
            model.spec(declaration, {**record, 'oracle_text': record['oracle_text'] + '\nDraw a card.'})
        with self.assertRaises(ValueError):
            model.spec({**declaration, 'variants': ['uneven_paid_1']}, record)

    def test_offturn_paid_retains_real_foe_main_and_pays_the_sorcery(self):
        for seat in (0, 1):
            game = self.game(model.ALMS, seat, 'offturn_paid')
            before = game.steps[:next(i for i, s in enumerate(game.steps) if s['op'] == 'activate')]
            self.assertEqual([s['player'] for s in before if s['op'] == 'turn'][-2:], [seat, 1 - seat])
            self.assertEqual([s['player'] for s in before if s['op'] == 'untap_step'][-2:], [seat, 1 - seat])
            self.assertEqual(game.life[seat], 23)
            self.assertEqual(game.life[1 - seat], 17)
            self.assertEqual(game.cards['source']['zone'], 'graveyard')

    def test_replay_repeats_exact_real_discard_and_effect_observers(self):
        for csvid in model.SPECS:
            game = self.game(csvid, 1, 'replay')
            by_label = {s['op']: i for i, s in enumerate(game.steps)}
            first = game.steps[by_label['capture'] + 1:by_label['record_state']]
            second = game.steps[by_label['restore'] + 1:by_label['assert_recorded_state']]
            self.assertEqual(first, second)
            self.assertEqual([(s['op'], s.get('ref')) for s in first if s['op'] == 'activate'],
                             [('activate', 'looter')])

    def test_hunter_checks_two_distinct_outcomes_after_real_regeneration_shield(self):
        for seat in (0, 1):
            game = self.game(model.HUNTER, seat, 'shielded_normal')
            self.assertEqual([s['ref'] for s in game.steps if s['op'] == 'cast'],
                             ['shield', 'source', 'destroy'])
            self.assertEqual([s['ref'] for s in game.steps if s['op'] == 'choose_target'], ['large', 'small'])
            self.assertEqual(game.cards['large']['zone'], 'graveyard')
            self.assertEqual(game.cards['small']['zone'], 'battlefield')
            self.assertTrue(any(s['op'] == 'assert_card' and s['ref'] == 'small'
                                and s['property'] == 'tapped' and s['value'] == 1 for s in game.steps))

    def test_asymmetric_eruption_counts_each_of_four_real_target_offers(self):
        for seat in (0, 1):
            game = self.game(model.ERUPTION, seat, 'uneven_paid')
            selected = [s for s in game.steps if s['op'] in ('choose_target', 'choose_player')]
            self.assertEqual([s.get('ref') for s in selected], ['wall', 'wall', 'wall', None])
            self.assertEqual(selected[-1]['value'], 1 - seat)
            self.assertEqual(game.life[1 - seat], 19)


if __name__ == '__main__':
    unittest.main()
