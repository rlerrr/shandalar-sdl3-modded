"""Guard Frilled Mystic's real optional target and fixed costs."""

import unittest

import flash_entry_stack_control_recipes as model


class FlashStackControlGuard(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        profiles = model.profiles()
        pins = profiles['sources'] | profiles['witnesses']
        cls.oracle = {name: pin['oracle'] for name, pin in pins.items()}
        cls.rows = {name: pin['registry'] for name, pin in pins.items()}
        cls.record = cls.oracle[model.FRILLED_NAME]

    def scenario(self, mode, seat):
        game = model.Duel(model.FRILLED, self.record, seat, mode, self.rows, self.oracle)
        game.build()
        return game

    def test_whole_printed_optional_trigger_and_color_fee_are_required(self):
        shape = model.classify(self.record, model.FRILLED)
        decl = dict(family=model.FAMILY, version=2, params=shape,
                    oracle_text=self.record['oracle_text'], variants=['low_0', 'decline_1'])
        self.assertEqual(model.spec(decl, self.record)['mana'], [0, 0, 2, 2, 0, 0])
        with self.assertRaises(ValueError):
            model.spec(decl, dict(self.record, oracle_text=self.record['oracle_text'].replace('may ', '')))
        with self.assertRaises(ValueError):
            model.spec(dict(decl, version=2.0), self.record)
        with self.assertRaises(ValueError):
            model.spec(dict(decl, variants=['empty_1']), self.record)

    def test_actual_paid_response_and_offer_happen_in_stack_order(self):
        for seat in (0, 1):
            game = self.scenario('low', seat)
            steps = game.steps
            announce = steps.index(dict(op='announce_spell', ref='parent'))
            cast = steps.index(dict(op='cast_response', ref='source', value=1))
            ask = steps.index(dict(op='choose_dialog', player=seat, value=0))
            self.assertLess(announce, cast)
            self.assertLess(cast, ask)
            self.assertLess(ask, steps.index(dict(op='resolve_response')))
            self.assertIn(dict(op='set_mana', player=seat, color=2, value=2), steps[announce:cast])
            self.assertIn(dict(op='set_mana', player=seat, color=3, value=2), steps[announce:cast])
            self.assertEqual(game.drawn, 0)

    def test_decline_preserves_the_original_spell_and_exact_draw(self):
        for seat in (0, 1):
            game = self.scenario('decline', seat)
            self.assertIn(dict(op='choose_dialog', player=seat, value=1), game.steps)
            self.assertEqual(game.cards['source']['zone'], 'battlefield')
            self.assertEqual(game.drawn, 1)
            self.assertEqual(game.cards[f'library_{1 - seat}_0']['zone'], 'hand')
            self.assertEqual(game.library[1 - seat], ['Forest', 'Plains', 'Mountain'])

    def test_legal_uncounterable_spell_stays_after_acceptance(self):
        for seat in (0, 1):
            game = self.scenario('uncounterable', seat)
            self.assertIn(dict(op='choose_dialog', player=seat, value=0), game.steps)
            self.assertEqual(game.cards['parent']['name'], model.UNC_SMALL)
            self.assertEqual(game.cards['parent']['zone'], 'battlefield')

    def test_empty_stack_flash_is_cast_then_countered_before_entry(self):
        for seat in (0, 1):
            game = self.scenario('empty_countered', seat)
            self.assertEqual(game.steps[0], dict(op='turn', player=1 - seat))
            self.assertIsNone(game.parent)
            self.assertIn(dict(op='cast', ref='source', value=1), game.steps)
            self.assertIn(dict(op='cast_response', ref='counter', value=1), game.steps)
            self.assertFalse(any(step['op'] in ('choose_dialog', 'choose_trigger') for step in game.steps))
            self.assertEqual(game.cards['source']['zone'], 'graveyard')
