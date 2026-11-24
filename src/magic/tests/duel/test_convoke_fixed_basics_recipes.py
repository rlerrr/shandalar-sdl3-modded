import json
from pathlib import Path
import unittest

import convoke_fixed_basics_recipes as recipe


class ConvokeFixedBasicsTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle = json.loads((Path(recipe.__file__).parent / 'data/oracle.json').read_bytes())['cards']
        pins = recipe.profiles()
        cls.rows = {name: pin['registry'] for group in pins.values() for name, pin in group.items()}
        cls.names = {cls.rows[name]['csvid']: name for name in pins['sources']}

    def play(self, csvid, mode, owner):
        game = recipe.ConvokeCase(self.oracle[self.names[csvid]], csvid, owner, mode, self.rows, self.oracle)
        game.play()
        return game

    def test_current_controller_can_convoke_with_opponent_owned_creature(self):
        for owner in (0, 1):
            game = self.play(7801, 'foreign_owned_mixed', owner)
            white = next(row for row in game.setup if row['ref'] == 'white')
            self.assertEqual(white['player'], owner)
            self.assertEqual(white['state'], ['sick', 'opponent_owned'])
            self.assertEqual(game.budget, (3, 0, 0, 0, 0, 1))
            self.assertTrue(game.cards['red']['tapped'] and game.cards['white']['tapped'])

    def test_convoke_stops_at_last_needed_contributor(self):
        for owner in (0, 1):
            game = self.play(19422, 'last_needed_contributor', owner)
            self.assertEqual(game.budget, recipe.ZERO)
            self.assertEqual(game.contributors, ['red_0', 'red_1', 'red_2', 'white'])
            self.assertTrue(all(game.cards[ref]['tapped'] for ref in game.contributors))
            self.assertFalse(game.cards['spare']['tapped'])
            self.assertEqual(sum(action['op'] == 'choose_target' for action in game.steps),
                             len(game.contributors) if owner == 0 else 0)

    def test_counter_keeps_actual_convoke_payment(self):
        for owner in (0, 1):
            game = self.play(19365, 'countered', owner)
            self.assertTrue(game.cards['red']['tapped'])
            self.assertEqual(game.mana, [recipe.ZERO, recipe.ZERO])
            self.assertEqual([game.cards[ref]['zone'] for ref in ('source', 'counter')], ['graveyard'] * 2)
            self.assertEqual(sum(action['op'] == 'cast_response' for action in game.steps), 1)

    def test_snapshot_forks_after_one_paid_cast(self):
        csvid = 14200
        name = self.names[csvid]
        record = self.oracle[name]
        shape = recipe.classify(record, csvid)
        for owner in (0, 1):
            chosen = [f'pending_replay_{owner}']
            declaration = dict(family=recipe.FAMILY, version=2, params=shape,
                               oracle_text=record['oracle_text'], variants=chosen)
            cases = recipe.expand(name, csvid, declaration, recipe.spec(declaration, record),
                                  chosen, self.rows, self.oracle)
            steps = cases[0]['steps']
            ops = [action['op'] for action in steps]
            self.assertEqual(ops.count('cast'), 1)
            self.assertLess(ops.index('cast'), ops.index('capture'))
            self.assertEqual(ops.count('resolve'), 2)
            self.assertLess(ops.index('restore'), ops.index('ai_roundtrip'))
            self.assertEqual(ops[-1], 'assert_recorded_state')
