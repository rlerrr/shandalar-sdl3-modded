"""Check the fixture boundaries around real conditional flips."""

import unittest

import coin_flip_conditional_activation_recipes as model


class ConditionalCoinGuard(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        profiles = model.profiles()
        pins = profiles['sources'] | profiles['witnesses']
        cls.oracle = {name: item['oracle'] for name, item in pins.items()}
        cls.rows = {name: item['registry'] for name, item in pins.items()}
        cls.by_id = {item['csvid']: name for name, item in cls.rows.items()}

    def game(self, csvid, seat, mode):
        game = model.Duel(csvid, self.oracle[self.by_id[csvid]], seat, mode, self.rows, self.oracle)
        game.build()
        return game

    def test_quoz_diagnostic_uses_real_upkeep_without_resolving_ante(self):
        for seat in (0, 1):
            game = self.game(model.QUOZ, seat, 'own_upkeep_probe')
            self.assertEqual([s['player'] for s in game.steps if s['op'] == 'upkeep_step'], [1-seat, seat])
            self.assertFalse(any(s['op'] in ('activate', 'resolve', 'choose_dialog') for s in game.steps))
            record = game.record
            with self.assertRaises(ValueError):
                model.classify({**record, 'oracle_text': record['oracle_text'].splitlines()[-1]}, model.QUOZ)

    def test_artisans_response_respects_pending_parent_and_player(self):
        for seat in (0, 1):
            for mode, allowed in (('win', 1), ('lose', 1), ('foe_artifact', 0), ('own_nonartifact', 0)):
                game = self.game(model.ARTISANS, seat, mode)
                scope = [s for s in game.steps if s['op'] in ('announce_spell', 'activate_response', 'resolve_response')]
                self.assertEqual([s['op'] for s in scope],
                                 ['announce_spell', 'activate_response'] + ['resolve_response'] * allowed)
                self.assertEqual(scope[1]['value'], allowed)
                self.assertEqual(game.cards['parent']['zone'], 'graveyard' if mode == 'lose' else 'battlefield')

    def test_delayed_kites_flip_follows_real_flight_combat(self):
        for seat in (0, 1):
            for mode in ('win', 'lose'):
                steps = self.game(model.KITES, seat, mode).steps
                coin = next(i for i, s in enumerate(steps) if s['op'] == 'choose_dialog')
                self.assertLess(next(i for i, s in enumerate(steps) if s['op'] == 'end_combat'), coin)
                self.assertEqual(steps[coin + 1]['op'], 'advance_turn')

    def test_box_copies_the_one_eligible_candidate_and_draws_for_each_heads(self):
        for seat in (0, 1):
            for mode, expected in model.EXPECTED_BOX_HEADS.items():
                game = self.game(model.BOX, seat, mode)
                self.assertEqual(model.box_heads(game.seed), expected)
                self.assertEqual([game.drawn[seat], game.drawn[1-seat]], list(map(int, expected)))
                self.assertEqual(sum(deck.count(model.COPY) for deck in game.library.values()), 1)
                self.assertEqual(sum(key.startswith('copy_') for key in game.cards), sum(expected))


if __name__ == '__main__':
    unittest.main()
