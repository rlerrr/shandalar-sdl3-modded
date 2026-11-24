"""Targeted guards for sacrifice inputs, printed riders and timing receipts."""

from copy import deepcopy
import unittest

import regeneration_follow_on_sacrifice_and_team_recipes as model


class RegenerationGuards(unittest.TestCase):
    def fixtures(self):
        merged = {}
        for group in model.profiles().values():
            merged.update(group)
        return ({k: v['oracle'] for k, v in merged.items()}, {k: v['registry'] for k, v in merged.items()})

    def board(self, name, mode, actor=0):
        oracle, rows = self.fixtures()
        board = model.FollowOn(oracle[name], rows[name]['csvid'], mode, actor, rows, oracle)
        board.play()
        return board

    def test_whole_source_and_werewolf_witness(self):
        oracle, rows = self.fixtures()
        model.validate_witnesses(oracle, rows)
        record = oracle['Vigilant Martyr']
        shape = model.classify(record, model.MARTYR)
        declaration = dict(family=model.FAMILY, version=2, oracle_text=record['oracle_text'],
                           params=shape, variants=model.variants(shape))
        self.assertEqual(model.spec(declaration, record)['csvid'], model.MARTYR)
        for bad in (dict(version=2.0), dict(variants=['counter_0', 'counter_0']), dict(variants=['counter_2'])):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                model.spec(declaration | bad, record)
        changed = deepcopy(oracle)
        changed['Tormented Pariah']['card_faces'][0]['power'] = '4'
        with self.assertRaises(ValueError):
            model.validate_witnesses(changed, rows)

    def test_paid_swamp_input_is_distinct_from_target(self):
        for seat, expected in ((0, ['cost_card','host']), (1, ['host'])):
            board = self.board('Horror of Horrors', 'friendly', seat)
            refs = [s['ref'] for s in board.steps if s['op'] == 'choose_target']
            self.assertEqual(refs, expected)
            self.assertEqual(board.cards['cost_card']['zone'], 'graveyard')
            self.assertEqual(board.cards['other_swamp']['zone'], 'battlefield')

    def test_vassal_retains_counter_after_regeneration(self):
        board = self.board('Gore Vassal', 'survives')
        self.assertEqual([s['value'] for s in board.steps if s['op'] == 'assert_counter'], [1, 1])
        self.assertEqual([s['ref'] for s in board.steps if s['op'] == 'destroy'], ['host', 'host'])
        zero = self.board('Gore Vassal', 'lethal')
        self.assertEqual(zero.cards['host']['zone'], 'graveyard')
        self.assertFalse(any(s['op'] == 'destroy' for s in zero.steps))

    def test_counter_spell_scope_and_paid_cost(self):
        for mode in ('counter', 'wrong_target', 'counter_sick', 'counter_short'):
            board = self.board('Vigilant Martyr', mode, 1)
            success = mode == 'counter'
            self.assertEqual(board.cards['protected']['zone'], 'battlefield' if success else 'graveyard')
            self.assertEqual(board.cards['source']['zone'], 'graveyard' if success else 'battlefield')
            self.assertEqual(any(s['op'] == 'activate_response' for s in board.steps), success)


if __name__ == '__main__':
    unittest.main()
