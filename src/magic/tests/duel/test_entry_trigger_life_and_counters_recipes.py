"""Validate exact Oracle records and paid entry fixture construction."""
from copy import deepcopy
import unittest

from entry_trigger_life_and_counters_recipes import (
    Board, BANSHEE, FAMILIAR, FAMILY, FIRSTBLADE, MEDICINE, MINOTAUR, SNAKE, SPHINX,
    TUSKER, WALKER, WHITE_KNIGHT, profiles, spec, classify, validate_witnesses, variants,
)


class LifeAndCountersTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        pinned = {**profiles()['sources'], **profiles()['witnesses']}
        cls.oracle = {name: row['oracle'] for name, row in pinned.items()}
        cls.rows = {name: row['registry'] for name, row in pinned.items()}
        cls.names = {row['registry']['csvid']: name for name, row in profiles()['sources'].items()}

    def case(self, csvid, mode, seat=0):
        board = Board(self.rows[self.names[csvid]], seat, mode, self.rows, self.oracle)
        board.run()
        return board

    def test_exact_pinned_and_typed(self):
        record = self.oracle[self.names[SNAKE]]
        shape = classify(record, SNAKE)
        recipe = dict(family=FAMILY, version=2, params=shape, variants=variants(shape), oracle_text=record['oracle_text'])
        self.assertEqual(spec(recipe, record), dict(shape, family=FAMILY))
        for bad in (dict(recipe, version=2.0), dict(recipe, params=dict(shape, csvid=float(SNAKE))),
                    dict(recipe, variants=['paid_0', 'paid_0'])):
            with self.assertRaises(ValueError):
                spec(bad, record)
        with self.assertRaises(ValueError):
            spec(recipe, dict(record, oracle_text=record['oracle_text']+' '))

    def test_witness_pin_includes_real_protection_color(self):
        validate_witnesses(self.oracle, self.rows)
        self.assertIn('Protection from black', self.oracle[WHITE_KNIGHT]['oracle_text'])
        changed = deepcopy(self.rows)
        changed[WHITE_KNIGHT]['iid'] -= 1
        with self.assertRaisesRegex(ValueError, WHITE_KNIGHT):
            validate_witnesses(self.oracle, changed)

    def test_short_and_artifact_observer(self):
        for seat in (0, 1):
            for csvid in (WALKER, SPHINX):
                case = self.case(csvid, 'short', seat)
                self.assertFalse(any(item['player'] == seat and item['zone'] == 'battlefield' for item in case.setup))
            case = self.case(WALKER, 'recast', seat)
            self.assertEqual(case.life[seat], 26)
            self.assertEqual(case.energy[seat], 6)
            self.assertEqual([op['index'] for op in case.steps if op['op'] == 'assert_matching_permanents'], [WALKER, WALKER])

    def test_haste_combat_before_cleanup(self):
        survives = self.case(FIRSTBLADE, 'blocked')
        dies = self.case(MINOTAUR, 'blocked', 1)
        for case in (survives, dies):
            acts = [op['op'] for op in case.steps]
            self.assertLess(acts.index('cast'), acts.index('begin_attackers'))
            self.assertLess(acts.index('combat_damage'), acts.index('advance_turn'))
            self.assertEqual(case.cards['blocker']['zone'], 'graveyard')
        self.assertEqual(survives.cards['source']['zone'], 'battlefield')
        self.assertEqual(dies.cards['source']['zone'], 'graveyard')

    def test_tribute_is_opponent_choice_and_recast_rechecks(self):
        for seat in (0, 1):
            case = self.case(SNAKE, 'recast', seat)
            self.assertEqual([(op['player'], op['value']) for op in case.steps if op['op'] == 'choose_dialog'],
                             [(1-seat, 1), (1-seat, 0)])
            self.assertEqual(case.life[seat], 24)

    def test_counter_kind_target_and_hybrid_payment(self):
        for mode, color, initial, final in (('white_charge', 5, 3, 2), ('green_enemy', 3, 2, 1)):
            case = self.case(MEDICINE, mode, 1)
            target = 'countered_artifact' if color == 5 else 'countered_creature'
            self.assertEqual([op['value'] for op in case.steps if op['op'] == 'assert_counter' and op['ref'] == target],
                             [initial, final])
            end = next(i for i, op in enumerate(case.steps) if op['op'] == 'cast' and op['ref'] == 'source')
            self.assertTrue(any(op['op'] == 'set_mana' and op['color'] == color and op['value'] == 1
                                for op in case.steps[end-8:end]))

    def test_death_draw_expiry_and_other_creature_scope(self):
        for seat in (0, 1):
            self.assertEqual(self.case(FAMILIAR, 'dies', seat).cards[f'deck_top_{seat}']['zone'], 'hand')
            self.assertEqual(self.case(FAMILIAR, 'bounce', seat).cards[f'deck_top_{seat}']['zone'], 'library')
            flying = self.case(BANSHEE, 'flying', seat)
            self.assertEqual(flying.life[1-seat], 18)
            tusker = self.case(TUSKER, 'recast', seat)
            for ref, expected in (('source', [0, 0]), ('own', [1, 2]), ('other', [0, 0])):
                self.assertEqual([op['value'] for op in tusker.steps if op['op'] == 'assert_counter'
                                  and op['ref'] == ref and op['counter_kind'] == 'plus1'], expected)
