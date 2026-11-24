"""Focused Oracle, paid rider and case-generation checks."""

from copy import deepcopy
import unittest

import remaining_entry_combat_recipes as model


class RemainingEntryCombatTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        pins = model.profiles()
        pinned = pins['sources'] | pins['witnesses']
        cls.oracle = {name: row['oracle'] for name, row in pinned.items()}
        cls.rows = {name: row['registry'] for name, row in pinned.items()}
        cls.names = {row['csvid']: name for name, row in cls.rows.items()}

    def board(self, csvid, mode, seat):
        selected = model.Follower if csvid in model.FOLLOWERS else model.Board
        board = selected(self.oracle[self.names[csvid]], csvid, mode, seat, self.rows, self.oracle)
        board.play()
        return board

    def test_rejects_changed_forecast_cost_and_duplicate_variant(self):
        record = self.oracle[self.names[model.HUSSAR]]
        params = model.classify(record, model.HUSSAR)
        declaration = dict(family=model.FAMILY, version=2, params=params, variants=model.variants(params),
                           oracle_text=record['oracle_text'])
        self.assertEqual(model.spec(declaration, record)['csvid'], model.HUSSAR)
        changed = deepcopy(record)
        changed['oracle_text'] = changed['oracle_text'].replace('Tap two', 'Tap one')
        with self.assertRaises(ValueError):
            model.spec(declaration, changed)
        with self.assertRaises(ValueError):
            model.spec(dict(declaration, variants=['untap_scope_0', 'untap_scope_0']), record)

    def test_fury_real_activation_has_oracle_damage_and_tap_receipts(self):
        board = self.board(model.FURY, 'player', 1)
        activation, = [s for s in board.steps if s['op']=='activate_granted_legacy']
        self.assertEqual(activation, dict(op='activate_granted_legacy', ref='ally', index=model.FURY, value=1))
        self.assertEqual(board.life, [18, 20])

    def test_hornet_condition_uses_only_controller_counters(self):
        own = self.board(model.HORNET, 'eligible', 0)
        foreign = self.board(model.HORNET, 'opponent_only', 0)
        self.assertEqual(own.cards['guard']['zone'], 'graveyard')
        self.assertEqual(foreign.cards['guard']['zone'], 'battlefield')

    def test_exarch_nonlethal_shrink_and_toughness_reset(self):
        board = self.board(model.EXARCH, 'expire', 1)
        self.assertEqual([s['value'] for s in board.steps if s.get('ref')=='guard' and
                          s.get('property')=='toughness'], [2, 4])

    def test_moray_pays_before_each_resolution_and_third_is_lethal(self):
        board = self.board(model.MORAY, 'third_dies', 0)
        self.assertEqual([s['value'] for s in board.steps if s['op']=='assert_energy' and s['player']==0], [4, 3, 2, 1])
        self.assertEqual(sum(s['op']=='activate' for s in board.steps), 3)
        self.assertEqual(board.cards['source']['zone'], 'graveyard')

    def test_gatekeeper_ignores_the_opponents_gate(self):
        own = self.board(model.GATEKEEPER, 'two', 1)
        foreign = self.board(model.GATEKEEPER, 'foreign', 1)
        self.assertEqual(own.cards['guard']['zone'], 'graveyard')
        self.assertEqual(foreign.cards['guard']['zone'], 'battlefield')


if __name__ == '__main__':
    unittest.main()
