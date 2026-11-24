"""Focused guards for real Suspend clocks and ordinary casting."""

import unittest

import suspend_paid_countdown_recipes as model


class PaidSuspendGuard(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        pinned = model.profiles()
        entries = pinned['sources'] | pinned['witnesses']
        cls.oracle = {name: data['oracle'] for name, data in entries.items()}
        cls.rows = {name: data['registry'] for name, data in entries.items()}
        cls.names = {row['csvid']: name for name, row in cls.rows.items()}

    def fixture_for(self, csvid, actor, mode):
        case = model.Duel(csvid, self.oracle[self.names[csvid]], actor, mode, self.rows, self.oracle)
        case.build()
        return case

    def test_full_text_and_no_normal_route_without_mana_cost(self):
        for csvid in model.CARDS:
            record = self.oracle[self.names[csvid]]
            shape = model.classify(record, csvid)
            with self.assertRaises(ValueError):
                model.classify({**record, 'oracle_text': record['oracle_text'] + '\nDraw a card.'}, csvid)
            self.assertEqual('normal_0' in model.variants(shape), bool(record['mana_cost']))
            self.assertEqual('unfunded_0' in model.variants(shape), any(shape['suspend']))

    def test_replay_ticks_actual_owner_and_opponent_upkeeps(self):
        for csvid in (8648, 8529):
            game = self.fixture_for(csvid, 1, 'replay')
            steps = game.steps
            capture = next(i for i, action in enumerate(steps) if action['op'] == 'capture')
            record = next(i for i, action in enumerate(steps) if action['op'] == 'record_state')
            restore = next(i for i, action in enumerate(steps) if action['op'] == 'restore')
            players = lambda actions: [s['player'] for s in actions if s['op'] == 'upkeep_step']
            self.assertEqual(players(steps[:record]), [0, 1] * game.duration)
            self.assertEqual(players(steps[capture:record]), players(steps[restore:]))
            self.assertGreater(capture, next(i for i, s in enumerate(steps) if s['op'] == 'cast'))

    def test_paid_removal_moves_exiled_card_before_final_upkeep(self):
        for actor in (0, 1):
            game = self.fixture_for(8477, actor, 'removed')
            steps = game.steps
            pull = next(i for i, s in enumerate(steps) if s['op'] == 'cast' and s.get('ref') == 'pull')
            choice = next(i for i, s in enumerate(steps) if s['op'] == 'choose_zone_card')
            final = max(i for i, s in enumerate(steps) if s['op'] == 'upkeep_step' and s['player'] == actor)
            self.assertLess(pull, choice)
            self.assertLess(choice, final)
            self.assertEqual(steps[choice]['value'], 8477)
            self.assertEqual(game.cards['source']['zone'], 'graveyard')
            self.assertEqual(game.cards['source']['player'], actor)
            self.assertEqual(len(game.deck[actor]), 4)
