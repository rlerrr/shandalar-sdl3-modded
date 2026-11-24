"""Focused guards for paid discard, off-turn Madness, and replay."""

import unittest

import madness_creature_casting_recipes as model


class MadnessCreatureGuard(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        pins = model.profiles()
        entries = pins['sources'] | pins['witnesses']
        cls.oracle = {name: pin['oracle'] for name, pin in entries.items()}
        cls.rows = {name: pin['registry'] for name, pin in entries.items()}
        cls.names = {entry['csvid']: name for name, entry in cls.rows.items()}

    def game(self, csvid, actor, mode):
        game = model.Duel(csvid, self.oracle[self.names[csvid]], actor, mode, self.rows, self.oracle)
        game.build()
        return game

    def test_full_text_and_actual_zero_cost_funding_scope(self):
        for csvid in model.CARDS:
            record = self.oracle[self.names[csvid]]
            shape = model.classify(record, csvid)
            declaration = dict(family=model.FAMILY, version=2, params=shape,
                               oracle_text=record['oracle_text'], variants=['paid_0', 'normal_1'])
            self.assertEqual(model.spec(declaration, record)['madness'], shape['madness'])
            self.assertEqual('short_1' in model.variants(shape), any(shape['madness']))
            with self.assertRaises(ValueError):
                model.classify({**record, 'oracle_text': record['oracle_text'] + '\nDraw a card.'}, csvid)

    def test_offturn_payment_occurs_in_the_actual_other_main_phase(self):
        for actor in (0, 1):
            game = self.game(5769, actor, 'offturn_paid')
            activate = next(i for i, step in enumerate(game.steps) if step['op'] == 'activate')
            phases = [step['player'] for step in game.steps[:activate] if step['op'] == 'turn']
            self.assertEqual(phases[-1], 1 - actor)
            self.assertTrue(any(step['op'] == 'untap_step' and step['player'] == 1 - actor
                                for step in game.steps[:activate]))
            self.assertEqual(game.cards['source']['zone'], 'battlefield')

    def test_replay_repeats_discard_and_countered_normal_skips_it(self):
        replay = self.game(16237, 1, 'replay').steps
        start = next(i for i, step in enumerate(replay) if step['op'] == 'capture')
        restore = next(i for i, step in enumerate(replay) if step['op'] == 'restore')
        for suffix in (replay[start:restore], replay[restore:]):
            self.assertEqual(sum(step['op'] == 'activate' and step.get('ref') == 'looter' for step in suffix), 1)
        countered = self.game(16237, 1, 'normal_countered')
        self.assertEqual(countered.cards['source']['zone'], 'graveyard')
        self.assertEqual([step['op'] for step in countered.steps if step['op'] in
                          ('cast', 'announce_spell', 'cast_response', 'resolve_response')],
                         ['cast', 'announce_spell', 'cast_response', 'resolve_response'])
        self.assertFalse(any(step['op'] == 'discard_policy' for step in countered.steps))
