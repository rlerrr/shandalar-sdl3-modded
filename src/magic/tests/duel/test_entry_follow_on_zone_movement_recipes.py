"""Guard whole Oracle pins and real follow-on zone movements."""

from copy import deepcopy
import unittest

from entry_follow_on_zone_movement_recipes import (
    BAFFLING, BINDING, DEPUTY, FAMILY, GUARDIAN, MODES, ORIGINAL, RIFT, SPHERE, STAG,
    Scene, classify, pins, spec, variants,
)


class EntryFollowOnZoneMovementTest(unittest.TestCase):
    def setUp(self):
        records = pins()['sources'] | pins()['witnesses']
        self.oracle = {name: item['oracle'] for name, item in records.items()}
        self.rows = {name: item['registry'] for name, item in records.items()}

    def scene(self, cid, mode, seat):
        row, = (row for row in self.rows.values() if row['csvid'] == cid)
        scene = Scene(row, seat, mode, self.rows, self.oracle)
        scene.play()
        return scene

    def test_full_exact_source_pin(self):
        self.assertEqual(len(MODES), 16)
        self.assertEqual(len(ORIGINAL), 4)
        for item in pins()['sources'].values():
            record, cid = item['oracle'], item['registry']['csvid']
            shape = classify(record, cid)
            decl = dict(family=FAMILY, version=2, params=shape, oracle_text=record['oracle_text'],
                        variants=variants(shape))
            self.assertEqual(spec(decl, record)['csvid'], cid)
            changed = deepcopy(record)
            changed['oracle_text'] += '\nDraw a card.'
            with self.assertRaises(ValueError):
                spec(decl, changed)

    def test_riftsweeper_face_down_predicate_built_from_real_courier(self):
        for seat in (0, 1):
            s = self.scene(RIFT, 'hidden_only', seat)
            self.assertEqual(s.shuffles, [0, 0])
            self.assertEqual(s.zones[seat]['exile'], ['own_top'])
            self.assertEqual([row[0] for row in s.steps].count('attack'), 1)
            t = self.scene(RIFT, 'hidden_plus_face_up', seat)
            self.assertEqual(t.shuffles[seat], 1)
            self.assertEqual(t.zones[seat]['hand'], ['own_top'])
            self.assertIn('courier', t.zones[seat]['graveyard'])

    def test_named_scope_distinguishes_all_permanents_from_opponent_copies(self):
        for seat in (0, 1):
            sphere = self.scene(SPHERE, 'same_name', seat)
            deputy = self.scene(DEPUTY, 'same_name', seat)
            cid = self.rows['Grizzly Bears']['csvid']

            def exile_receipts(s, p):
                return [row[5] for row in s.steps if row[0] == 'assert_zone_multiplicity'
                        and row[4] == p and row[6:8] == (4, cid)]

            self.assertEqual(exile_receipts(sphere, seat), [1, 0])
            self.assertEqual(exile_receipts(deputy, seat), [0, 0])
            self.assertEqual(exile_receipts(deputy, 1-seat), [2, 0])

    def test_binding_removal_reenables_actual_named_cast(self):
        for seat in (0, 1):
            s = self.scene(BINDING, 'cast_lock', seat)
            self.assertIn(('can_cast', 'foe_match', '', '', 0, 0, 0, 0, 0), s.steps)
            actual_casts = [row[1] for row in s.steps if row[0] == 'cast']
            self.assertEqual(actual_casts, ['source', 'foe_other', 'remover', 'foe_match'])

    def test_guardian_owner_and_baffling_token_recipients(self):
        for seat in (0, 1):
            guardian = self.scene(GUARDIAN, 'borrowed', seat)
            self.assertIn('target', guardian.zones[1-seat]['battlefield'])
            baffling = self.scene(BAFFLING, 'token_trample', seat)
            self.assertIn('target', baffling.zones[1-seat]['exile'])
            self.assertIn('dinosaur', baffling.zones[1-seat]['battlefield'])
            self.assertEqual(baffling.life[seat], 19)
            stag = self.scene(STAG, 'vigilance', seat)
            self.assertIn(('assert_card', 'source', '', 'tapped', 0, 0, 0, 0, 0), stag.steps)
            self.assertEqual(stag.life[1-seat], 17)


if __name__ == '__main__':
    unittest.main()
