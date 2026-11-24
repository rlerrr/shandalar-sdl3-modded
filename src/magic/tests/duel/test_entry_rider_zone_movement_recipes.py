"""Guard full pinned records and meaningful composed zone effects."""

from copy import deepcopy
import unittest

from entry_rider_zone_movement_recipes import (
    BLOODWATER, DANCE, DWELLERS, FAMILY, HAG, MODES, NECROMANCY, NUCKLAVEE, ORIGINAL,
    SERENITY, SKULLWINDER, Scene, classify, pins, source_cost, spec, variants,
)


class EntryRiderZoneMovementTest(unittest.TestCase):
    def setUp(self):
        data = pins()['sources'] | pins()['witnesses']
        self.oracle = {name: rec['oracle'] for name, rec in data.items()}
        self.rows = {name: rec['registry'] for name, rec in data.items()}

    def case(self, cid, label, seat):
        row, = (r for r in self.rows.values() if r['csvid'] == cid)
        scene = Scene(row, seat, label, self.rows, self.oracle)
        scene.play()
        return scene

    def test_exact_source_records_and_coverage(self):
        self.assertEqual((len(MODES), len(ORIGINAL)), (16, 4))
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

    def test_three_zone_ceiling_and_actual_return_to_hand(self):
        for owner in (0, 1):
            s = self.case(SERENITY, 'mixed', owner)
            self.assertEqual(s.zones[owner]['hand'], ['grave_0'])
            self.assertEqual(s.zones[1-owner]['hand'], ['permanent_0', 'grave_1'])
            t = self.case(SERENITY, 'limit_grave', owner)
            self.assertEqual(t.zones[owner]['graveyard'], ['grave_3', 'remover', 'source'])
            self.assertIn(('assert_unused_choices', '', '', '', 0, 1, 0, 0, 0), t.steps)

    def test_actual_timing_and_upkeep_payment(self):
        for owner in (0, 1):
            s = self.case(DANCE, 'upkeep_pay', owner)
            self.assertEqual([step[4] for step in s.steps if step[0] == 'upkeep_step'], [1-owner, owner])
            self.assertEqual([step[5] for step in s.steps if step[:4] == ('assert_card', 'target', '', 'tapped')],
                             [1, 1, 1, 0])
            n = self.case(NECROMANCY, 'flash_cleanup', owner)
            self.assertEqual([step[4] for step in n.steps if step[0] == 'turn'], [owner, 1-owner])
            self.assertIn('target', n.zones[owner]['graveyard'])
            ordinary = self.case(NECROMANCY, 'ordinary_persists', owner)
            self.assertEqual(ordinary.zones[owner]['battlefield'], ['source', 'target'])

    def test_hybrid_and_distinct_return_slots(self):
        self.assertEqual(source_cost(self.oracle['Desecrator Hag'], HAG, 'tie'), [2, 0, 0, 2, 0, 0])
        self.assertEqual(source_cost(self.oracle['Nucklavee'], NUCKLAVEE, 'red_only'), [4, 0, 0, 0, 2, 0])
        for owner in (0, 1):
            n = self.case(NUCKLAVEE, 'red_only', owner)
            self.assertEqual(n.zones[owner]['hand'], ['red'])
            self.assertIn('blue', n.zones[owner]['graveyard'])
            s = self.case(SKULLWINDER, 'both_graves', owner)
            self.assertEqual([step[4] for step in s.steps if step[0] == 'choose_zone_card'], [owner, 1-owner])

    def test_free_cast_and_prowess_witnesses_are_real(self):
        for owner in (0, 1):
            s = self.case(DWELLERS, 'free_three', owner)
            self.assertEqual(s.zones[owner]['exile'], ['target'])
            self.assertEqual(s.zones[owner]['hand'], ['own_top', 'next_draw'])
            p = self.case(BLOODWATER, 'prowess_flying', owner)
            self.assertEqual([step[1] for step in p.steps if step[0] == 'cast'], ['source', 'boost'])
            self.assertEqual([step[5] for step in p.steps if step[:4] == ('assert_card', 'source', '', 'power')],
                             [2, 3, 2])
            self.assertEqual(p.life[1-owner], 18)


if __name__ == '__main__':
    unittest.main()
