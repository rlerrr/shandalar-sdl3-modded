"""Guard exact records and independently exercised attack riders."""
from copy import deepcopy
import unittest

from attack_follow_on_counters_and_dethrone_recipes import (
    Board, BRAWLER, CUTTHROAT, EMISSARY, FAMILY, INFILTRATOR, OGRE, ROC,
    ZOMBIE_A, classify, profiles, spec, validate_witnesses, variants,
)


class CounterRiderTests(unittest.TestCase):
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

    def test_exact_typed_source_and_version(self):
        record = self.oracle[self.names[ROC]]
        shape = classify(record, ROC)
        decl = dict(family=FAMILY, version=2, params=shape, variants=variants(shape), oracle_text=record['oracle_text'])
        self.assertEqual(spec(decl, record), dict(shape, family=FAMILY))
        with self.assertRaises(ValueError):
            spec(decl, dict(record, oracle_text=record['oracle_text'] + ' '))
        for bad in (dict(decl, version=2.0), dict(decl, params=dict(shape, csvid=float(ROC))),
                    dict(decl, variants=['decline_1']), dict(decl, variants=['pay_target_0', 'pay_target_0'])):
            with self.assertRaises(ValueError):
                spec(bad, record)

    def test_exact_witness_records(self):
        validate_witnesses(self.oracle, self.rows)
        changed = deepcopy(self.rows)
        changed[ZOMBIE_A]['iid'] -= 1
        with self.assertRaisesRegex(ValueError, ZOMBIE_A):
            validate_witnesses(self.oracle, changed)

    def test_real_partial_amplify_and_block_life_loss(self):
        case = self.case(BRAWLER, 'manual_one')
        self.assertEqual([op['value'] for op in case.steps if op['op'] == 'choose_zone_card'],
                         [self.rows[ZOMBIE_A]['csvid']])
        self.assertEqual(case.life, [19, 17])
        self.assertEqual(self.case(BRAWLER, 'manual_none').life, [20, 18])
        self.assertEqual(self.case(BRAWLER, 'auto_block', 1).life, [20, 18])

    def test_first_strike_and_hexproof_controls(self):
        first = self.case(CUTTHROAT, 'first_strike', 1)
        self.assertEqual((first.cards['source']['zone'], first.cards['enemy']['zone']), ('battlefield', 'graveyard'))
        for seat in (0, 1):
            case = self.case(EMISSARY, 'hexproof', seat)
            self.assertEqual([op['value'] for op in case.steps if op['op'] == 'can_target_creature'], [1, 1, 0, 1])

    def test_infiltrator_combat_damage_draw(self):
        for mode, count in (('tie', 1), ('blocked', 0), ('fog', 0)):
            case = self.case(INFILTRATOR, mode, 1)
            observed = [op for op in case.steps if op['op'] == 'assert_zone_card']
            self.assertEqual(len(observed), 4 - count)
            self.assertEqual(case.cards['deck_1_0']['zone'], 'hand' if count else 'library')

    def test_ogre_mana_pays_for_the_real_spell(self):
        for seat in (0, 1):
            case = self.case(OGRE, 'mana_dethrone', seat)
            start = next(i for i, op in enumerate(case.steps) if op['op'] == 'activate' and op['ref'] == 'source')
            end = next(i for i, op in enumerate(case.steps) if op['op'] == 'cast' and op['ref'] == 'life_action')
            self.assertFalse(any(op['op'] == 'set_mana' for op in case.steps[start:end]))
            self.assertEqual((case.life[seat], case.life[1-seat]), (17, 17))
            self.assertEqual(self.case(OGRE, 'short_life', seat).life[seat], 2)

    def test_roc_real_entry_scope_and_attack_target_boundary(self):
        for mode, energy, tapped in (('pay_target', 0, [1]), ('no_target', 0, []), ('foe_entry', 1, [0]),
                                     ('decline', 2, [0]), ('skip_target', 0, [0])):
            case = self.case(ROC, mode)
            self.assertEqual([op['value'] for op in case.steps if op['op'] == 'assert_energy' and op['player'] == 0][-1], energy)
            self.assertEqual([op['value'] for op in case.steps if op['op'] == 'assert_card'
                              and op['ref'] == 'victim' and op['property'] == 'tapped'], tapped)
