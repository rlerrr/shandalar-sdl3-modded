"""Focused guards for preserved coin inputs and new distinct outcomes."""

import unittest

import coin_flip_repeated_flips_recipes as model


class RepeatedCoinGuard(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        pins = model.profiles()
        entries = pins['sources'] | pins['witnesses']
        cls.oracle = {name: pin['oracle'] for name, pin in entries.items()}
        cls.rows = {name: pin['registry'] for name, pin in entries.items()}

    def game(self, csvid, mode, actor=0):
        game = model.Duel(csvid, actor, mode, self.rows, self.oracle)
        game.build()
        return game

    def test_complete_records_and_typed_declarations(self):
        model.validate_witnesses(self.oracle, self.rows)
        for csvid, (name, _, _) in model.CARDS.items():
            record = self.oracle[name]
            shape = model.classify(record, csvid)
            declaration = dict(family=model.FAMILY, version=2, params=shape,
                               oracle_text=record['oracle_text'], variants=model.variants(shape))
            self.assertEqual(model.spec(declaration, record)['csvid'], csvid)
            with self.assertRaises(ValueError):
                model.spec(declaration, {**record, 'oracle_text': record['oracle_text'] + '\nDraw a card.'})
            with self.assertRaises(ValueError):
                model.spec({**declaration, 'version': True}, record)

    def test_real_fizzle_keeps_tapped_lands_and_no_flips(self):
        game = self.game(6664, 'target_fizzle')
        self.assertCountEqual([step['ref'] for step in game.steps if step['op'] == 'activate'],
                              ['mana0', 'mana1', 'mana2'])
        self.assertEqual([step['op'] for step in game.steps if step['op'] in
                          ('cast', 'announce_spell', 'cast_response', 'resolve_response', 'choose_dialog')],
                         ['cast', 'announce_spell', 'cast_response', 'announce_spell', 'resolve_response'])
        self.assertEqual(game.cards['target'][2], 'hand')
        self.assertEqual(game.cards['source'][2], 'graveyard')
        self.assertEqual(game.draws, [0, 0])
        self.assertEqual(game.life, [20, 20])

    def test_clash_prevention_uses_identical_asymmetric_rounds(self):
        for actor in (0, 1):
            raw = self.game(343, 'asymmetric', actor)
            armor = self.game(343, 'armor', actor)
            expected = [(1, 1), (1, 0), (1, 0), (0, 0)]
            self.assertEqual(raw.rounds, expected)
            self.assertEqual(armor.rounds, expected)
            self.assertEqual((raw.life[actor], armor.life[actor]), (17, 20))
            self.assertEqual(raw.life[1 - actor], armor.life[1 - actor])

    def test_repeated_revenge_and_independent_thumb_choices(self):
        for actor in (0, 1):
            revenge = self.game(5437, 'win_three', actor)
            thumb = self.game(6693, 'keep_twice', actor)
            self.assertEqual(revenge.draws[actor], 6)
            self.assertEqual([step['value'] for step in revenge.steps if step['op'] == 'choose_number'], [3])
            self.assertEqual(thumb.draws[actor], 4)
            self.assertEqual([step['value'] for step in thumb.steps if step['op'] == 'choose_number'], [2])
            self.assertEqual([step['value'] for step in thumb.steps if step['op'] == 'choose_dialog'], [0, 1, 0, 1])
