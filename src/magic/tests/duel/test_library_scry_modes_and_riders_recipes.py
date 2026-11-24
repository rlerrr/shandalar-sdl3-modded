"""Guard the exact pinned scry shapes and their real action receipts."""
from copy import deepcopy
import unittest

from library_scry_modes_and_riders_recipes import (
    ALIGNMENT, ANCESTRY, BRAIN, COPY, EXCAVATIONS, EYE, FAMILY, INSIGHT, JACE_SECRET, NISSA, SAHEELI,
    Board, classify, profiles, spec, validate_witnesses, variants,
)


class LibraryScryModesAndRidersTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        pinned = {**profiles()['witnesses'], **profiles()['sources']}
        cls.oracle = {name: pin['oracle'] for name, pin in pinned.items()}
        cls.rows = {name: pin['registry'] for name, pin in pinned.items()}
        cls.names = {pin['registry']['csvid']: name for name, pin in profiles()['sources'].items()}

    def run_case(self, csvid, mode, seat=0):
        board = Board(self.rows[self.names[csvid]], seat, mode, self.rows, self.oracle)
        board.run()
        return board.steps

    def test_complete_primary_and_exact_typed_declaration(self):
        record = self.oracle[self.names[COPY]]
        shape = classify(record, COPY)
        declaration = dict(family=FAMILY, version=2, oracle_text=record['oracle_text'], params=shape,
                           variants=variants(shape))
        self.assertEqual(spec(declaration, record), dict(shape, family=FAMILY))
        with self.assertRaisesRegex(ValueError, 'exact pinned'):
            spec(declaration, dict(record, oracle_text='Scry 2.'))
        for mutation in (dict(declaration, version=2.0), dict(declaration, params=dict(shape, csvid=float(COPY))),
                         dict(declaration, variants=['reorder_1']), dict(declaration, variants=['own_0', 'own_0'])):
            with self.assertRaises(ValueError):
                spec(mutation, record)

    def test_source_and_witness_pins_include_registered_identity(self):
        validate_witnesses(self.oracle, self.rows)
        damaged = deepcopy(self.rows)
        damaged['Seal of Cleansing']['iid'] += 1
        with self.assertRaisesRegex(ValueError, 'Seal of Cleansing'):
            validate_witnesses(self.oracle, damaged)

    def test_counterspell_uses_parent_context_without_staged_scry(self):
        for csvid in (13707, 13708, 13962):
            for seat in (0, 1):
                steps = self.run_case(csvid, 'countered', seat)
                self.assertIn('announce_spell', [step['op'] for step in steps])
                self.assertIn('resolve_response', [step['op'] for step in steps])
                self.assertFalse(any(step['op'].startswith('choose_') for step in steps))

    def test_scry_draw_uses_enchanted_creature_controller(self):
        steps = self.run_case(INSIGHT, 'opposing_host')
        self.assertTrue(any(step['op'] == 'choose_zone_card' and step['player'] == 1 for step in steps))
        self.assertTrue(any(step['op'] == 'assert_zone_multiplicity' and step['zone'] == 'hand'
                            and step['player'] == 1 and step['value'] == 1 for step in steps))
        self.assertTrue(any(step['op'] == 'assert_card' and step['ref'] == 'host'
                            and step['property'] == 'tapped' and step['value'] == 1 for step in steps))

    def test_land_entry_respects_island_and_real_colorless_blue_mana(self):
        paid, absent = [self.run_case(EXCAVATIONS, mode) for mode in ('mana', 'tapped_island')]
        self.assertTrue(any(step['op'] == 'choose_target' and step['ref'] == 'island' for step in paid))
        self.assertFalse(any(step['op'] == 'activate' for step in absent))
        self.assertEqual({step['color'] for step in paid if step['op'] == 'assert_mana' and step['value'] == 1}, {0, 2})
        self.assertTrue(any(step['op'] == 'play_land' for step in self.run_case(ANCESTRY, 'entry')))

    def test_hedron_terminal_gate_is_only_the_four_zone_mode(self):
        for seat in (0, 1):
            for mode in ('aligned', 'missing_exile'):
                steps = self.run_case(ALIGNMENT, mode, seat)
                self.assertTrue(any(step['op'] == 'upkeep_step' and step['player'] == seat for step in steps))
                self.assertEqual([step['value'] for step in steps if step['op'] == 'advance_scheduled_turn'],
                                 [-1] if mode == 'aligned' else [])

    def test_real_loyalty_followers_and_brain_modes(self):
        for csvid, mode, action in ((JACE_SECRET, 'emblem', 'announce_spell'), (SAHEELI, 'copy', 'attack'),
                                  (SAHEELI, 'search', 'assert_shuffle_count'), (NISSA, 'animate', 'block'),
                                  (BRAIN, 'scry', 'assert_scry_result')):
            steps = self.run_case(csvid, mode)
            self.assertIn(action, [step['op'] for step in steps])
            self.assertTrue(any(step['op'] == 'assert_counter' for step in steps))

    def test_sole_legal_activated_modes_still_have_explicit_inputs(self):
        for seat in (0, 1):
            for csvid, mode, expected in ((EYE, 'off_turn', [0, 1]), (EYE, 'detach', [0]),
                                           (BRAIN, 'free_cast', [0]), (BRAIN, 'scry', [0, 1])):
                choices = [step['value'] for step in self.run_case(csvid, mode, seat)
                           if step['op'] == 'choose_dialog' and step['player'] == seat]
                self.assertEqual(choices, expected)


if __name__ == '__main__':
    unittest.main()
