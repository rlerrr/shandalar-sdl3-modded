"""Focused whole-Oracle and action guards for activated coin outcomes."""
from copy import deepcopy
import unittest

from coin_flip_activated_outcomes_recipes import (
    BANG, FAMILY, LYRE, MANA, SCALES, SEEDS, Board, classify, profiles, spec, validate_witnesses, variants,
)


def inputs():
    pinned = profiles()
    oracle = {name: value['oracle'] for items in pinned.values() for name, value in items.items()}
    records = {name: value['registry'] for items in pinned.values() for name, value in items.items()}
    return oracle, records


def case(csvid, actor, mode):
    oracle, rows = inputs()
    name = next(name for name in profiles()['sources'] if rows[name]['csvid'] == csvid)
    board = Board(name, actor, mode, rows, oracle, actor)
    board.build()
    return board


class ActivatedCoinGuards(unittest.TestCase):
    def test_whole_record_and_typed_variant(self):
        oracle, _ = inputs()
        record = oracle['Crooked Scales']
        shape = classify(record, SCALES)
        declaration = dict(family=FAMILY, version=2, params=shape, oracle_text=record['oracle_text'],
                           variants=variants(shape))
        self.assertEqual(spec(declaration, record)['csvid'], SCALES)
        with self.assertRaises(ValueError):
            spec(declaration, dict(record, oracle_text=record['oracle_text'].replace('{3}', '{2}')))
        for bad in (dict(declaration, family='different'), dict(declaration, version=2.0),
                    dict(declaration, variants=['win_0', 'win_0'])):
            with self.assertRaises(ValueError):
                spec(bad, record)

    def test_exact_witness_registry(self):
        oracle, rows = inputs()
        validate_witnesses(oracle, rows)
        modified = deepcopy(rows)
        modified['Cho-Manno, Revolutionary']['iid'] += 1
        with self.assertRaises(ValueError):
            validate_witnesses(oracle, modified)

    def test_native_seed_prefix(self):
        for face, seed in enumerate(SEEDS):
            for _ in range(16):
                seed ^= (seed << 13) & 0xffffffff
                seed ^= seed >> 17
                seed ^= (seed << 5) & 0xffffffff
                self.assertEqual(seed & 1, face)

    def test_scales_replay_preserves_both_paid_stages(self):
        for owner in (0, 1):
            board = case(SCALES, owner, 'replay')
            self.assertEqual(board.flips, [dict(call=1-owner, planned_win=False), dict(call=owner, planned_win=True)])
            sequence = [item['op'] for item in board.steps]
            self.assertLess(sequence.index('activate'), sequence.index('capture'))
            self.assertEqual([item['value'] for item in board.steps if item['op'] == 'choose_dialog'],
                             [1-owner, 0, owner])
            self.assertIn('assert_recorded_state', sequence)
            self.assertEqual(board.pools[owner], [0] * 6)

    def test_bangchuckers_own_target_and_prevention(self):
        for owner in (0, 1):
            own, prevented = (case(BANG, owner, mode) for mode in ('win_own', 'win_prevented'))
            self.assertEqual(own.cards['foe_mark']['player'], owner)
            self.assertEqual(own.cards['foe_mark']['zone'], 'graveyard')
            self.assertEqual(prevented.cards['foe_mark']['zone'], 'battlefield')
            self.assertTrue(any(s['op'] == 'assert_card' and s.get('ref') == 'foe_mark'
                                and s.get('property') == 'damage' and s['value'] == 0 for s in prevented.steps))

    def test_lyre_actual_zero_creature_scopes(self):
        for owner in (0, 1):
            for mode, seat in (('win_zero', owner), ('lose_zero', 1-owner)):
                board = case(LYRE, owner, mode)
                self.assertFalse(any(row['player'] == seat and row['zone'] == 'battlefield' for row in board.setup))
                self.assertEqual(board.cards['source']['zone'], 'graveyard')
                self.assertEqual(board.life, [20, 20])

    def test_mana_output_and_payment_menu_separation(self):
        for owner in (0, 1):
            paid = case(MANA, owner, 'colored_fee')
            self.assertEqual([s['ref'] for s in paid.steps if s['op'] == 'activate'], ['fee_land', 'source'])
            self.assertTrue(any(s['player'] == owner and s['color'] == 3 and s['value'] == 1
                                for s in paid.steps if s['op'] == 'assert_mana'))
            self.assertEqual(paid.cards['sink']['zone'], 'battlefield')
            self.assertEqual([(s['player'], s['color']) for s in paid.steps if s['op'] == 'choose_mana'],
                             [(0, 3)] if owner == 0 else [])
            if owner == 0:
                selection = next(i for i, s in enumerate(paid.steps) if s['op'] == 'choose_mana')
                self.assertEqual(paid.steps[selection + 1]['op'], 'choose_dialog')
            for mode, stack, output in (('win', 0, 2), ('lose', 0, 0), ('response_win', 1, 2),
                                        ('colored_fee', 0, 2)):
                actions = case(MANA, owner, mode).steps
                use = next(i for i, action in enumerate(actions)
                           if action['op'] in ('activate', 'activate_response') and action.get('ref') == 'source')
                self.assertEqual((actions[use + 1]['op'], actions[use + 1]['value']), ('assert_stack', stack))
                self.assertEqual((actions[use + 2]['op'], actions[use + 2]['player'],
                                  actions[use + 2]['color'], actions[use + 2]['value']),
                                 ('assert_mana', owner, 0, output))
            gate = case(MANA, owner, 'payment_gate')
            self.assertFalse(any(s['op'] == 'activate' for s in gate.steps))
            self.assertTrue(any(s.get('ref') == 'sink' and s['value'] == 0 for s in gate.steps if s['op'] == 'cast'))

