"""Guard pinned regeneration costs, protection, and printed riders."""
from copy import deepcopy
import unittest

from regeneration_self_and_riders_recipes import (
    Board, FAMILY, HEDGE, HUNTED, MUCK, THRUN, TROLL, classify, profiles, spec,
    validate_witnesses, variants,
)


class RegenerationSelfAndRidersTests(unittest.TestCase):
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
        record = self.oracle[self.names[MUCK]]
        shape = classify(record, MUCK)
        recipe = dict(family=FAMILY, version=2, oracle_text=record['oracle_text'], params=shape, variants=variants(shape))
        self.assertEqual(spec(recipe, record), dict(shape, family=FAMILY))
        with self.assertRaises(ValueError):
            spec(recipe, dict(record, oracle_text='Regenerate this creature.'))
        for change in (dict(recipe, version=2.0), dict(recipe, params=dict(shape, csvid=float(MUCK))),
                       dict(recipe, variants=['regenerate_0', 'regenerate_0'])):
            with self.assertRaises(ValueError):
                spec(change, record)

    def test_actual_witness_registry_is_pinned(self):
        validate_witnesses(self.oracle, self.rows)
        altered = deepcopy(self.rows)
        altered['Faerie']['iid'] -= 1
        with self.assertRaisesRegex(ValueError, 'Faerie'):
            validate_witnesses(self.oracle, altered)

    def test_actual_land_control_appearance_and_loss(self):
        for csvid, printed_power in ((MUCK, 2), (HEDGE, 2)):
            steps = self.case(csvid, 'type_enter_leave').steps
            self.assertEqual([item['value'] for item in steps if item['op'] == 'assert_card'
                              and item['ref'] == 'source' and item['property'] == 'power'],
                             [printed_power, printed_power+1, printed_power])
            self.assertIn('play_land', [item['op'] for item in steps])
            self.assertTrue(any(item['op'] == 'choose_target' and item['ref'] == 'my_type' for item in steps))

    def test_paid_reactive_and_proactive_receipts_are_distinct(self):
        for mode in ('regenerate', 'short', 'proactive'):
            case = self.case(HEDGE, mode)
            actions = [item['op'] for item in case.steps]
            if mode == 'proactive':
                self.assertIn('can_activate', actions)
                self.assertNotIn('choose_regeneration', actions)
            else:
                self.assertIn('choose_regeneration', actions)
                self.assertEqual(case.cards['source']['zone'], 'battlefield' if mode == 'regenerate' else 'graveyard')

    def test_thrun_paid_counter_and_nontarget_wipes(self):
        counter = self.case(THRUN, 'uncounterable', 1)
        self.assertTrue(any(item['op'] == 'cast_response' and item['ref'] == 'counterspell' for item in counter.steps))
        self.assertEqual(counter.cards['source']['zone'], 'battlefield')
        for mode, upkeep in (('regenerate', 'Day of Judgment'), ('terminate', 'Wrath of God')):
            case = self.case(THRUN, mode)
            self.assertEqual(case.cards['removal']['name'], upkeep)
            self.assertFalse(any(item['op'] == 'choose_target' for item in case.steps))

    def test_real_opponent_tokens_and_printed_flying(self):
        for owner in (0, 1):
            case = self.case(HUNTED, 'token_flight', owner)
            copies = [item for item in case.steps if item['op'] == 'bind_permanent_copy']
            self.assertEqual([(item['player'], item['index']) for item in copies], [(1-owner, i) for i in range(4)])
            self.assertTrue(any(item['op'] == 'block' and item['value'] == 0 for item in case.steps))
            self.assertEqual(case.life[owner], 19)
            shielded = self.case(HUNTED, 'protected_opponent', owner)
            self.assertFalse(any(item['op'] == 'bind_permanent_copy' for item in shielded.steps))

    def test_echo_is_only_paid_on_first_own_upkeep(self):
        result = self.case(TROLL, 'pay_once')
        self.assertEqual(sum(step['op'] == 'upkeep_step' for step in result.steps), 4)
        self.assertEqual(sum(step['op'] == 'choose_dialog' for step in result.steps), 1)
        self.assertEqual(sum(step['op'] == 'choose_mana_source' for step in result.steps), 2)


if __name__ == '__main__':
    unittest.main()
