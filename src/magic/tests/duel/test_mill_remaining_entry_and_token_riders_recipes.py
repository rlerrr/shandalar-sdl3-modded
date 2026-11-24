"""Focused guards for printed riders and optional mill actions."""

import copy
import unittest

import mill_remaining_entry_and_token_riders_recipes as model


class MillRiderTests(unittest.TestCase):
    def setUp(self):
        pins = model.profiles()
        all_pins = pins['sources'] | pins['witnesses'] | pins['tokens']
        self.oracle = {name: entry['oracle'] for name, entry in all_pins.items()}
        self.rows = {name: entry['registry'] for name, entry in all_pins.items()}

    def game(self, name, mode, seat):
        csvid = self.rows[name]['csvid']
        board = model.Duel(csvid, self.oracle[name], seat, mode, self.rows, self.oracle)
        board.build()
        return board

    def test_printed_records_and_token_bindings_fail_closed(self):
        model.validate_witnesses(self.oracle, self.rows)
        oracle = copy.deepcopy(self.oracle)
        oracle['Stitcher Geralf']['oracle_text'] = oracle['Stitcher Geralf']['oracle_text'].replace('up to two', 'two')
        with self.assertRaises(ValueError):
            model.validate_witnesses(oracle, self.rows)
        rows = copy.deepcopy(self.rows)
        rows['Clue']['csvid'] += 1
        with self.assertRaises(ValueError):
            model.validate_witnesses(self.oracle, rows)

    def test_declaration_rejects_float_version_or_unknown_variant(self):
        card = self.oracle['Fleeting Memories']
        body = dict(family=model.FAMILY, version=2, oracle_text=card['oracle_text'],
                    params=model.classify(card, model.MEMORIES), variants=['self_1'])
        self.assertEqual(model.spec(body, card)['csvid'], model.MEMORIES)
        for change in (dict(version=2.0), dict(variants=['self_2']), dict(variants=['self_1', 'self_1'])):
            with self.assertRaises(ValueError):
                model.spec(body | change, card)

    def test_milled_creatures_set_exact_token_size_and_old_grave_stays(self):
        for seat in (0, 1):
            board = self.game('Stitcher Geralf', 'two', seat)
            self.assertEqual(board.cards['old_creature']['zone'], 'graveyard')
            self.assertEqual(len(board.mill_events), 2)
            self.assertEqual({item['name'] for item in board.cards.values() if item['zone'] == 'exile'},
                             {model.DRAGON, model.ZOMBIE})
            self.assertIn(dict(op='assert_matching_permanents', property='power', player=seat,
                               index=self.rows['Zombie']['csvid'], value=7), board.steps)
            self.assertIn(dict(op='assert_matching_permanents', property='color', player=seat,
                               index=self.rows['Zombie']['csvid'], value=4), board.steps)

    def test_self_mill_precedes_clue_draw_and_foe_clue_is_independent(self):
        for seat in (0, 1):
            board = self.game('Fleeting Memories', 'self', seat)
            self.assertEqual(board.mill_events[0]['cards'], model.STOCK[:3])
            self.assertEqual(board.cards[f'library_{seat}_3']['zone'], 'hand')
            self.assertEqual(board.cards['clue']['zone'], 'gone')
            self.assertIn(dict(op='choose_player', player=seat, value=seat), board.steps)
            other = self.game('Fleeting Memories', 'other_clue', seat)
            self.assertEqual(other.cards['clue']['zone'], 'battlefield')
            self.assertEqual(other.cards['foe_clue']['zone'], 'gone')
            self.assertFalse(other.mill_events)
            self.assertFalse(any(item['op'] == 'choose_player' for item in other.steps))
