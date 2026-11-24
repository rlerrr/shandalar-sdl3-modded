"""Guard exact card records and scenario-specific inputs for zone movement."""

from copy import deepcopy
import unittest

from entry_trigger_zone_movement_recipes import (
    ANGEL, BOG, CHAINED, CORPSE, DEADWOOD, FAMILY, HEDRON, MODES, ORIGINAL, RAMPAGER,
    SIBSIG, TIDECALLER, Scene, classify, pins, spec, variants,
)


class EntryTriggerZoneMovementTest(unittest.TestCase):
    def setUp(self):
        records = pins()['sources'] | pins()['witnesses']
        self.oracle = {name: record['oracle'] for name, record in records.items()}
        self.rows = {name: record['registry'] for name, record in records.items()}

    def scene(self, cid, mode, seat):
        row, = (row for row in self.rows.values() if row['csvid'] == cid)
        scene = Scene(row, seat, mode, self.rows, self.oracle)
        scene.play()
        return scene

    def test_complete_record_pin_and_scope(self):
        self.assertEqual(len(MODES), 16)
        self.assertEqual(len(ORIGINAL), 4)
        for name, pin in pins()['sources'].items():
            cid, record = pin['registry']['csvid'], pin['oracle']
            shape = classify(record, cid)
            payload = dict(version=2, family=FAMILY, params=shape, oracle_text=record['oracle_text'],
                           variants=variants(shape))
            self.assertEqual(spec(payload, record)['csvid'], cid)
            modified = deepcopy(record)
            modified['oracle_text'] += '\nDraw a card.'
            with self.assertRaises(ValueError):
                spec(payload, modified)

    def test_bog_seeds_follow_slot_chronology(self):
        for seat in (0, 1):
            s = self.scene(BOG, 'opponent_grave', seat)
            actual = [(row[7], row[5]) for row in s.steps
                      if row[0] == 'assert_zone_card' and row[4] == seat and row[6] == 3]
            expected = list(enumerate(self.rows[n]['csvid'] for n in ('Forest', 'Lightning Bolt', 'Hill Giant')))
            self.assertEqual(actual, expected + expected)

    def test_payment_is_real_and_precedes_zone_recovery(self):
        for seat in (0, 1):
            s = self.scene(SIBSIG, 'delve_return', seat)
            ops = [row[0] for row in s.steps]
            self.assertEqual(ops[:ops.index('cast')].count('choose_zone_card'), 8)
            self.assertEqual(len(s.zones[seat]['exile']), 8)
            self.assertEqual(s.zones[seat]['hand'], ['recover'])
            self.assertEqual(s.zones[1-seat]['graveyard'], ['foe_creature'])
            r = self.scene(RAMPAGER, 'pay_two', seat)
            self.assertEqual([row[5] for row in r.steps if row[0] == 'assert_energy' and row[4] == seat],
                             [0, 1, 2, 0])
            self.assertEqual([row[0] for row in r.steps].count('cast'), 3)

    def test_return_and_expiry_preserve_owner_and_timing(self):
        for seat in (0, 1):
            h = self.scene(HEDRON, 'return_owner', seat)
            self.assertIn('high', h.zones[1-seat]['battlefield'])
            c = self.scene(CHAINED, 'host_destroyed', seat)
            self.assertEqual(c.zones[seat]['graveyard'], ['remove', 'mountain', 'source'])
            self.assertIn('enemy', c.zones[1-seat]['battlefield'])
            v = self.scene(DEADWOOD, 'vanish', seat)
            self.assertEqual([row[5] for row in v.steps if row[0] == 'assert_counter'], [3, 3, 2, 2, 1, 1])
            self.assertIn('source', v.zones[seat]['graveyard'])
            self.assertEqual(v.zones[seat]['hand'], ['incoming', 'outgoing'])

    def test_literal_tribes_poison_and_flying_witnesses(self):
        for seat in (0, 1):
            a = self.scene(ANGEL, 'mixed_tribes', seat)
            self.assertEqual(a.zones[seat]['exile'], ['own_zombie'])
            self.assertIn('foe_human', a.zones[1-seat]['graveyard'])
            self.assertEqual(a.life[seat], 23)
            s = self.scene(CORPSE, 'poison', seat)
            self.assertIn(('assert_poison', '', '', '', 1-seat, 2, 0, 0, 0), s.steps)
            fly = self.scene(TIDECALLER, 'own_land_flying', seat)
            self.assertEqual(fly.life[1-seat], 19)
            ground = self.scene(TIDECALLER, 'opponent_land_ground', seat)
            self.assertIn('land_creature', ground.zones[1-seat]['graveyard'])


if __name__ == '__main__':
    unittest.main()
