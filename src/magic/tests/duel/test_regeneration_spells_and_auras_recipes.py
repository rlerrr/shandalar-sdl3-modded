"""Check printed clauses and high-risk input and outcome distinctions."""

from copy import deepcopy
import unittest

import regeneration_spells_and_auras_recipes as model


class RegenerationGuards(unittest.TestCase):
    def fixtures(self):
        result = {}
        for group in model.profiles().values():
            result.update(group)
        return ({k: v['oracle'] for k, v in result.items()}, {k: v['registry'] for k, v in result.items()})

    def declaration(self, name):
        oracle, _ = self.fixtures()
        record = oracle[name]
        source = model.profiles()['sources'][name]
        shape = model.classify(record, source['registry']['csvid'])
        return record, dict(family=model.FAMILY, version=2, oracle_text=record['oracle_text'],
                            params=shape, variants=model.variants(shape))

    def test_complete_oracle_and_distinct_choices(self):
        record, declaration = self.declaration('Golgari Charm')
        self.assertEqual(model.spec(declaration, record)['csvid'], model.CHARM)
        changed = deepcopy(record)
        changed['oracle_text'] += '\nDraw a card.'
        with self.assertRaises(ValueError):
            model.spec(declaration, changed)
        for bad in (dict(version=2.0), dict(variants=['weaken_0', 'weaken_0']),
                    dict(variants=['weaken_2']), dict(family='wrap')):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                model.spec(declaration | bad, record)

    def test_witness_provenance(self):
        oracle, rows = self.fixtures()
        model.validate_witnesses(oracle, rows)
        oracle['Elvish Mystic'] = oracle['Elvish Mystic'] | {'toughness': '2'}
        with self.assertRaises(ValueError):
            model.validate_witnesses(oracle, rows)

    def play(self, name, mode, seat=0):
        oracle, rows = self.fixtures()
        csvid = rows[name]['csvid']
        board = model.FollowOn(oracle[name], csvid, mode, seat, rows, oracle)
        board.play()
        return board

    def test_entwine_keeps_extra_black_and_own_shield_semantics(self):
        board = self.play('Wail of the Nim', 'entwine')
        self.assertIn(('choose_dialog', 2), [(x['op'], x['value']) for x in board.steps])
        payments = [step['value'] for step in board.steps if step['op'] == 'set_mana' and step['color'] == 1]
        self.assertIn(2, payments)
        self.assertEqual(board.life, [19, 19])
        self.assertEqual(board.cards['contrast']['zone'], 'graveyard')
        self.assertEqual(board.cards['second']['zone'], 'graveyard')
        self.assertTrue(any(step['op'] == 'assert_card' and step.get('ref') == 'second' and
                            step.get('property') == 'tapped' and step['value'] == 1 for step in board.steps))

    def test_discard_selection_and_return_are_bound_to_actual_seats(self):
        board = self.play('Strands of Undeath', 'self_discards')
        self.assertEqual([step['player'] for step in board.steps if step['op'] == 'choose_zone_card'], [0, 0])
        ai = self.play('Strands of Undeath', 'self_discards', 1)
        self.assertFalse(any(step['op'] == 'choose_zone_card' for step in ai.steps))
        both = self.play('Broken Fall', 'recast', 1)
        self.assertEqual([step['player'] for step in both.steps if step['op'] == 'bind_hand'], [1])
        self.assertEqual(both.cards['source']['zone'], 'hand')

    def test_charm_human_passes_ordinary_priority_before_regeneration(self):
        for seat in (0, 1):
            board = self.play('Golgari Charm', 'regenerate', seat)
            choices = [(step.get('ref'), step['value']) for step in board.steps if step['op'] == 'choose_priority']
            self.assertEqual(choices, [(None, -2), ('source', 0)] if seat == 0 else [('source', 0)])
            self.assertEqual(board.cards['host']['zone'], 'battlefield')
            self.assertEqual(board.cards['second']['zone'], 'battlefield')
            self.assertEqual(board.cards['contrast']['zone'], 'graveyard')


if __name__ == '__main__':
    unittest.main()
