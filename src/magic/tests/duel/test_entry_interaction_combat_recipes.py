"""Focused guards for new interaction paths and exact Oracle scope."""

from copy import deepcopy
import unittest

import entry_interaction_combat_recipes as model


class EntryInteractionCombatTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        pinned = model.profiles()['sources'] | model.profiles()['witnesses']
        cls.oracle = {name: p['oracle'] for name, p in pinned.items()}
        cls.rows = {name: p['registry'] for name, p in pinned.items()}
        cls.names = {row['csvid']: name for name, row in cls.rows.items()}

    def board(self, csvid, mode, actor):
        game = model.CompanionBoard(self.oracle[self.names[csvid]], csvid, mode, actor, self.rows, self.oracle)
        game.play()
        return game

    def test_full_oracle_rejects_changed_duration_and_variants(self):
        record = self.oracle[self.names[model.LEVIATHAN]]
        params = model.classify(record, model.LEVIATHAN)
        request = dict(family=model.FAMILY, version=2, params=params, variants=model.variants(params),
                       oracle_text=record['oracle_text'])
        self.assertEqual(model.spec(request, record)['csvid'], model.LEVIATHAN)
        changed = deepcopy(record)
        changed['oracle_text'] = changed['oracle_text'].replace('next untap steps', 'untap steps')
        with self.assertRaises(ValueError):
            model.spec(request, changed)
        with self.assertRaises(ValueError):
            model.spec(dict(request, variants=['hand_0', 'hand_0']), record)

    def test_real_reanimation_never_casts_the_creature_from_graveyard(self):
        board = self.board(model.LEVIATHAN, 'reanimate', 0)
        self.assertEqual([step['ref'] for step in board.steps if step['op'] == 'cast'], ['raise'])
        self.assertEqual(board.cards['source']['zone'], 'battlefield')
        self.assertEqual([step['value'] for step in board.steps if step.get('ref') == 'foe_red' and
                          step.get('property') == 'tapped'], [0])

    def test_leviathan_tap_restriction_is_one_untap_per_seat(self):
        board = self.board(model.LEVIATHAN, 'one_untap', 0)
        self.assertEqual([step['player'] for step in board.steps if step['op'] == 'untap_step'], [1, 0, 1, 0])
        for ref in ('own_red', 'foe_red'):
            self.assertEqual([step['value'] for step in board.steps if step.get('ref') == ref and
                              step.get('property') == 'tapped'], [1, 1, 0])

    def test_surge_uses_an_actual_same_turn_cast_and_leaves_source_unmodified(self):
        board = self.board(model.BUSHWHACKER, 'surge', 1)
        actions = [(s['op'], s.get('ref')) for s in board.steps if s['op'] in ('cast', 'announce_spell', 'attack')]
        self.assertEqual(actions, [('cast', 'ally'), ('announce_spell', 'ally'), ('cast', 'source'), ('attack', 'ally')])
        self.assertEqual([s['value'] for s in board.steps if s.get('ref') == 'source' and s.get('property') == 'power'], [2])
        self.assertEqual([s['value'] for s in board.steps if s.get('ref') == 'ally' and s.get('property') == 'power'], [2, 1])

    def test_ambition_uses_separate_recipient_and_grants_real_lifelink(self):
        board = self.board(model.AMBITION, 'own_counter', 0)
        self.assertEqual([s['ref'] for s in board.steps if s['op'] == 'choose_target'], ['ally', 'counter_recipient'])
        self.assertEqual(board.life, [22, 18])

    def test_trial_recasts_same_object_only_after_controlled_cartouche(self):
        for mode, expected in (('own_cartouche', ['source', 'cartouche', 'source']),
                               ('foe_cartouche', ['source', 'cartouche'])):
            board = self.board(model.TRIAL, mode, 1)
            self.assertEqual([s['ref'] for s in board.steps if s['op'] == 'cast'], expected)
            self.assertEqual(board.cards['source']['zone'], 'battlefield')


if __name__ == '__main__':
    unittest.main()
