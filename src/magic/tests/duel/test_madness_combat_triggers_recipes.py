"""Focused guards on real discard, combat receipts and independent Oracle costs."""

from copy import deepcopy
import unittest

from madness_combat_triggers_recipes import FAMILY, TriggerDuel, classify, profiles, spec, variants


class MadnessCombatTriggerTests(unittest.TestCase):
    def setUp(self):
        pins = profiles()['sources'] | profiles()['witnesses']
        self.rows = {n: pin['registry'] for n, pin in pins.items()}
        self.oracle = {n: pin['oracle'] for n, pin in pins.items()}

    def board(self, name, seat, mode):
        game = TriggerDuel(self.rows[name]['csvid'], self.oracle[name], seat, mode, self.rows, self.oracle)
        game.build()
        return game

    def test_paid_offturn_gorgon_can_block_while_fresh(self):
        game = self.board('Gorgon Recluse', 1, 'offturn_paid')
        activate = next(i for i, s in enumerate(game.steps) if s['op'] == 'activate')
        self.assertEqual([s['player'] for s in game.steps[:activate] if s['op'] == 'turn'][-1], 0)
        blocked, = (s for s in game.steps[activate:] if s['op'] == 'block')
        self.assertEqual((blocked['ref'], blocked['other'], blocked['value']), ('played', 'witness', 1))
        self.assertEqual(game.cards['witness']['zone'], 'graveyard')

    def test_occultist_replays_payment_combat_and_exiled_land_play(self):
        game = self.board('Stromkirk Occultist', 0, 'replay')
        self.assertEqual(sum(s['op'] == 'combat_damage' for s in game.steps), 2)
        self.assertEqual(sum(s['op'] == 'play_exiled' for s in game.steps), 2)
        self.assertEqual([s['op'] for s in game.steps].count('capture'), 1)
        self.assertEqual(game.cards['d_0_1']['zone'], 'battlefield')

    def test_exact_madness_fee_guard(self):
        record = self.oracle['Stensia Masquerade']
        model = classify(record, 15970)
        declaration = dict(family=FAMILY, version=2, params=model, oracle_text=record['oracle_text'],
                           variants=variants(model))
        self.assertEqual(spec(declaration, record)['madness'], [2, 0, 0, 0, 1, 0])
        changed = deepcopy(declaration)
        changed['params']['madness'][0] -= 1
        with self.assertRaises(ValueError):
            spec(changed, record)
