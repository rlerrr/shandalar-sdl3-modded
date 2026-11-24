"""Guard exact pinned sources and independent printed regeneration riders."""
from copy import deepcopy
import unittest

from regeneration_follow_on_self_and_riders_recipes import (
    Board, CHAMPION, ERON, FAMILY, KATHARI, MORTIVORE, PYGMY, RHOX, TRESSER,
    classify, profiles, spec, validate_witnesses, variants,
)


class RegenerationFollowOnSelfAndRidersTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        pins = {**profiles()['sources'], **profiles()['witnesses']}
        cls.oracle = {name: pin['oracle'] for name, pin in pins.items()}
        cls.rows = {name: pin['registry'] for name, pin in pins.items()}
        cls.names = {pin['registry']['csvid']: name for name, pin in profiles()['sources'].items()}

    def case(self, csvid, mode, owner=0):
        board = Board(self.rows[self.names[csvid]], owner, mode, self.rows, self.oracle)
        board.run()
        return board

    def test_whole_oracle_and_typed_recipe(self):
        record = self.oracle[self.names[ERON]]
        shape = classify(record, ERON)
        recipe = dict(family=FAMILY, version=2, oracle_text=record['oracle_text'], params=shape, variants=variants(shape))
        self.assertEqual(spec(recipe, record), dict(shape, family=FAMILY))
        with self.assertRaises(ValueError):
            spec(recipe, dict(record, oracle_text='Regenerate this creature.'))
        for mutation in (dict(recipe, version=2.0), dict(recipe, params=dict(shape, csvid=float(ERON))),
                         dict(recipe, variants=['haste_0', 'haste_0'])):
            with self.assertRaises(ValueError):
                spec(mutation, record)

    def test_source_and_used_witness_registry(self):
        validate_witnesses(self.oracle, self.rows)
        altered = deepcopy(self.rows)
        altered['Prey Upon']['iid'] -= 1
        with self.assertRaisesRegex(ValueError, 'Prey Upon'):
            validate_witnesses(self.oracle, altered)

    def test_champion_restores_the_same_chosen_player(self):
        for owner in (0, 1):
            case = self.case(CHAMPION, 'linked_self', owner)
            self.assertEqual(case.life, [20, 20])
            self.assertEqual(case.cards['source']['zone'], 'hand')
            self.assertEqual([item['value'] for item in case.steps if item['op'] == 'choose_player'], [owner])
            self.assertTrue(any(item['op'] == 'assert_life' and item['player'] == owner and item['value'] == 14
                                for item in case.steps))
            self.assertEqual(self.case(CHAMPION, 'regenerate', owner).life[1-owner], 14)

    def test_tresserhorn_partial_sacrifice_and_exact_draw(self):
        for mode, sacrifice_count in (('one_fodder', 2), ('zero_fodder', 1), ('protected', 0)):
            case = self.case(TRESSER, mode)
            self.assertEqual(sum(card['zone'] == 'graveyard' for card in case.cards.values()), sacrifice_count)
            self.assertEqual([case.cards[f'deck_1_{n}']['zone'] for n in (0, 1)],
                             ['library'] * 2 if mode == 'protected' else ['hand'] * 2)
            self.assertEqual(case.life[0], 20 if mode == 'protected' else 18)

    def test_mortivore_recomputes_real_graves(self):
        growth = self.case(MORTIVORE, 'growth')
        self.assertEqual([item['value'] for item in growth.steps if item['op'] == 'assert_card'
                          and item['ref'] == 'source' and item['property'] == 'power'], [2, 3, 4])
        self.assertEqual(self.case(MORTIVORE, 'clear_foe_grave').cards['foe_grave_creature']['zone'], 'exile')
        self.assertEqual(self.case(MORTIVORE, 'empty').cards['source']['zone'], 'graveyard')

    def test_blocked_pump_and_bypass_are_real_separate_receipts(self):
        blocked = self.case(PYGMY, 'blocked')
        self.assertEqual([item['value'] for item in blocked.steps if item['op'] == 'assert_card'
                          and item['ref'] == 'source' and item['property'] == 'power'], [1, 2, 1])
        for mode, enemy_life, zone in (('normal', 20, 'graveyard'), ('bypass', 15, 'battlefield')):
            case = self.case(RHOX, mode, 1)
            self.assertEqual((case.life[0], case.cards['blocking_giant']['zone']), (enemy_life, zone))
            self.assertTrue(any(item['op'] == 'choose_dialog' and item['value'] == int(mode == 'bypass')
                                for item in case.steps))

    def test_pilot_cascade_remains_cast_and_zero_exile(self):
        for owner in (0, 1):
            case = self.case(KATHARI, 'cascade_countered', owner)
            self.assertEqual(case.cards['source']['zone'], 'graveyard')
            self.assertEqual(case.cards[f'deck_{owner}_2']['zone'], 'battlefield')
            self.assertTrue(any(item['op'] == 'assert_zone' and item['player'] == owner
                                and item['zone'] == 'exile' and item['value'] == 0 for item in case.steps))


if __name__ == '__main__':
    unittest.main()
