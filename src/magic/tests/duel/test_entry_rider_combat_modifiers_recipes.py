"""Focused Oracle and distinct combat receipt guards for entry modifiers."""

from copy import deepcopy
import unittest

import entry_rider_combat_modifiers_recipes as model


class EntryCombatModifiers(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        pins = model.profiles()
        pinned = pins['sources'] | pins['witnesses']
        cls.oracle = {name: row['oracle'] for name, row in pinned.items()}
        cls.rows = {name: row['registry'] for name, row in pinned.items()}
        cls.names = {row['csvid']: name for name, row in cls.rows.items()}

    def declaration(self, card):
        record = self.oracle[self.names[card]]
        classified = model.classify(record, card)
        return record, dict(family=model.FAMILY, version=2, params=classified,
                            variants=model.variants(classified), oracle_text=record['oracle_text'])

    def board(self, card, mode, actor):
        board = model.FollowOn(self.oracle[self.names[card]], card, mode, actor, self.rows, self.oracle)
        board.play()
        return board

    def test_exact_record_and_distinct_variant_guard(self):
        record, declaration = self.declaration(model.HAMMER)
        self.assertEqual(model.spec(declaration, record)['csvid'], model.HAMMER)
        changed = deepcopy(record)
        changed['oracle_text'] = changed['oracle_text'].replace(' and has haste', '')
        with self.assertRaises(ValueError):
            model.spec(declaration, changed)
        with self.assertRaises(ValueError):
            model.spec(dict(declaration, variants=['host_haste_0', 'host_haste_0']), record)
        with self.assertRaises(ValueError):
            model.spec(dict(declaration, version=True), record)

    def test_aura_cast_target_and_entry_target_are_independent(self):
        board = self.board(model.HAMMER, 'separate_target', 1)
        actions = [(op['op'], op.get('ref')) for op in board.steps
                   if op['op'] in ('choose_target', 'cast', 'resolve')]
        self.assertEqual(actions[:4], [('choose_target', 'ally'), ('cast', 'source'),
                                       ('choose_target', 'other'), ('resolve', None)])
        self.assertIn(dict(op='block', ref='other', other='ally', player=0, value=0,
                           zone='hand', index=0, color=0), board.steps)

    def test_two_one_and_zero_distinct_block_targets(self):
        for mode, targets in (('two', ['guard', 'second_guard']), ('one', ['guard', None]), ('decline', [None])):
            with self.subTest(mode=mode):
                board = self.board(model.WARLORD, mode, 0)
                self.assertEqual([step.get('ref') for step in board.steps if step['op'] in (
                    'choose_target', 'choose_cancel_target')], targets)
                self.assertEqual([step['ref'] for step in board.steps if step['op'] == 'block' and not step['value']],
                                 [target for target in targets if target is not None])

    def test_first_strike_kills_before_return_damage(self):
        board = self.board(model.SPIRES, 'own', 1)
        self.assertEqual(board.cards['guard']['zone'], 'graveyard')
        self.assertEqual(board.cards['ally']['zone'], 'battlefield')
        self.assertEqual([op['value'] for op in board.steps if op['op'] == 'assert_card'
                          and op.get('ref') == 'ally' and op.get('property') == 'damage'], [0])

    def test_evoked_source_departs_but_grant_stays_until_cleanup(self):
        for actor in (0, 1):
            board = self.board(model.ACOLYTE, 'evoke', actor)
            self.assertEqual(board.cards['source']['zone'], 'graveyard')
            self.assertEqual(board.life[1-actor], 17)
            self.assertEqual([step['value'] for step in board.steps if step['op'] == 'assert_card'
                              and step.get('ref') == 'ally' and step.get('property') == 'power'], [3, 1])


if __name__ == '__main__':
    unittest.main()
