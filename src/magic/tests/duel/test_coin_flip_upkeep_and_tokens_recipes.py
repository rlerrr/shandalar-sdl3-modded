"""Focused guards for the distinct upkeep and token coin-flip variants."""

from copy import deepcopy
import unittest

from coin_flip_upkeep_and_tokens_recipes import (
    BOARDS, FAMILY, classify, profiles, spec, variants,
)


class CoinFlipUpkeepTokensTests(unittest.TestCase):
    def setUp(self):
        pins = profiles()['sources'] | profiles()['witnesses']
        self.rows = {name: row['registry'] for name, row in pins.items()}
        self.oracle = {name: row['oracle'] for name, row in pins.items()}

    def board(self, name, mode, owner):
        csvid = self.rows[name]['csvid']
        board = BOARDS[csvid](self.oracle[name], csvid, owner, mode, self.rows, self.oracle)
        board.build()
        return board

    def test_bomb_requires_five_wins_after_a_loss_at_zero(self):
        for owner in (0, 1):
            board = self.board('Goblin Bomb', 'loss_at_zero', owner)
            self.assertEqual([entry['expected_win'] for entry in board.coin_calls], [False] + [True] * 5)
            eligibility = [s['value'] for s in board.steps if s['op'] == 'can_activate']
            self.assertEqual(eligibility, [0] * 12 + [1])

    def test_mine_checks_red_payment_and_exact_nonlethal_damage(self):
        board = self.board('Goblin Kaboomist', 'mine_nonlethal', 0)
        self.assertEqual([s['value'] for s in board.steps if s['op'] == 'can_activate'], [0, 0, 1])
        self.assertEqual([s['value'] for s in board.steps if s.get('property') == 'damage'], [0, 2])
        self.assertEqual([s['ref'] for s in board.steps if s['op'] == 'activate'], ['mine'])

    def test_removed_march_does_not_prevent_existing_copy_attack_or_expiry(self):
        board = self.board('Mirror March', 'source_gone', 1)
        ops = [s['op'] for s in board.steps]
        removal = next(i for i, step in enumerate(board.steps) if step.get('ref') == 'removal')
        self.assertLess(removal, ops.index('attack'))
        self.assertLess(ops.index('attack'), ops.index('cleanup'))
        self.assertEqual([step['value'] for step in board.steps if step['op'] == 'attack'], [0, 1])

    def test_whole_card_source_and_declaration_must_match(self):
        record = self.oracle['Rakdos, the Showstopper']
        model = classify(record, 19703)
        declaration = dict(family=FAMILY, version=2, params=model, oracle_text=record['oracle_text'],
                           variants=variants(model))
        self.assertEqual(spec(declaration, record)['csvid'], 19703)
        wrong_cost = deepcopy(declaration)
        wrong_cost['params']['price'][0] -= 1
        with self.assertRaises(ValueError):
            spec(wrong_cost, record)
        with self.assertRaises(ValueError):
            spec(declaration, dict(record, keywords=['Flying']))
