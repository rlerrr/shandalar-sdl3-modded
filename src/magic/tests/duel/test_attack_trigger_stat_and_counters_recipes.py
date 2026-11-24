"""Guard whole-card attack declarations and distinct printed riders."""
from copy import deepcopy
import unittest

from attack_trigger_stat_and_counters_recipes import (
    Board, CAPTAIN, EXEMPLAR, FAMILY, HERALD, HYDRA, LIEUTENANT, MORTIS, OOZE, RAIDERS, WITCH,
    classify, profiles, spec, validate_witnesses, variants,
)


class AttackTriggerStatAndCountersTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        pins = {**profiles()['sources'], **profiles()['witnesses']}
        cls.oracle = {name: pin['oracle'] for name, pin in pins.items()}
        cls.rows = {name: pin['registry'] for name, pin in pins.items()}
        cls.names = {pin['registry']['csvid']: name for name, pin in profiles()['sources'].items()}

    def case(self, csvid, mode, seat=0):
        board = Board(self.rows[self.names[csvid]], seat, mode, self.rows, self.oracle)
        board.run()
        return board

    def test_exact_primary_and_typed_recipe(self):
        record = self.oracle[self.names[RAIDERS]]
        shape = classify(record, RAIDERS)
        decl = dict(family=FAMILY, version=2, params=shape, variants=variants(shape), oracle_text=record['oracle_text'])
        self.assertEqual(spec(decl, record), dict(shape, family=FAMILY))
        with self.assertRaises(ValueError):
            spec(decl, dict(record, oracle_text='Whenever this creature attacks, add a counter.'))
        for mutation in (dict(decl, version=2.0), dict(decl, params=dict(shape, csvid=float(RAIDERS))),
                         dict(decl, variants=['team_0', 'team_0'])):
            with self.assertRaises(ValueError):
                spec(mutation, record)

    def test_witnesses_include_exact_typed_cards(self):
        validate_witnesses(self.oracle, self.rows)
        changed = deepcopy(self.rows)
        changed['Arcbound Worker']['iid'] -= 1
        with self.assertRaisesRegex(ValueError, 'Arcbound Worker'):
            validate_witnesses(self.oracle, changed)

    def test_hydra_doubles_real_counters_only_for_owned_creatures(self):
        for owner in (0, 1):
            board = self.case(HYDRA, 'double', owner)
            for ref, counts in (('source', [4, 8, 8]), ('home_worker', [1, 2]), ('away_worker', [1, 1]), ('zero_friend', [0])):
                self.assertEqual([s['value'] for s in board.steps if s['op'] == 'assert_counter' and s['ref'] == ref], counts)
            self.assertTrue(all(board.cards[ref]['zone'] == 'battlefield' for ref in ('home_worker', 'away_worker')))

    def test_herald_uses_exact_owner_opponent_and_scaled_costs(self):
        for mode, generic in (('own_costs', 2), ('opponent_cost', 3), ('two_counters', 1)):
            board = self.case(HERALD, mode)
            cast = next(i for i, item in enumerate(board.steps) if item['op'] == 'cast' and item['ref'] == 'angel_spell')
            self.assertEqual([s['value'] for s in board.steps[cast-7:cast] if s['op'] == 'set_mana'], [generic, 0, 0, 0, 0, 2])

    def test_bolster_tie_and_exemplar_remove_exactly_one(self):
        case = self.case(CAPTAIN, 'tied_shroud', 1)
        self.assertTrue(any(s['op'] == 'choose_target' and s['ref'] == 'small_ally' for s in case.steps))
        self.assertEqual(self.case(EXEMPLAR, 'repeat').life, [23, 7])
        self.assertEqual(self.case(EXEMPLAR, 'ally_entry').life, [20, 16])

    def test_witch_threshold_and_optional_policies(self):
        self.assertEqual(self.case(WITCH, 'threshold').life, [20, 18])
        self.assertEqual(self.case(WITCH, 'below').life, [20, 20])
        for csvid, mode in ((WITCH, 'decline'), (MORTIS, 'self_target')):
            variants_for_source = variants(classify(self.oracle[self.names[csvid]], csvid))
            self.assertIn(mode + '_0', variants_for_source)
            self.assertNotIn(mode + '_1', variants_for_source)

    def test_death_and_renown_riders_distinguish_actual_outcomes(self):
        for mode, count in (('death_combat', 2), ('later_death', 2), ('different_victim', 1)):
            steps = self.case(OOZE, mode).steps
            self.assertEqual([s['value'] for s in steps if s['op'] == 'assert_counter'][-1], count)
        self.assertEqual(self.case(LIEUTENANT, 'renown_team').life, [20, 10])
        self.assertEqual(self.case(MORTIS, 'combat_death').life, [20, 16])


if __name__ == '__main__':
    unittest.main()
