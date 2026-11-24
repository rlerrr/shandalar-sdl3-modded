"""Guard actual upkeep routing, zone conditions, and return witnesses."""

from copy import deepcopy
import unittest

from upkeep_zone_movement_recipes import (
    BLOOM, COILS, CRUSHER, EQUIPOISE, FAMILY, ICHORID, MIST, MODES, MORTAL, ORIGINAL, SERVITOR, STINGS,
    Scene, classify, pins, spec, variants,
)


class UpkeepZoneMovementTest(unittest.TestCase):
    def setUp(self):
        fields = pins()['sources'] | pins()['witnesses']
        self.oracle = {name: item['oracle'] for name, item in fields.items()}
        self.rows = {name: item['registry'] for name, item in fields.items()}

    def scene(self, cid, mode, actor):
        row, = (row for row in self.rows.values() if row['csvid'] == cid)
        result = Scene(row, actor, mode, self.rows, self.oracle)
        result.play()
        return result

    def test_pinned_whole_records(self):
        self.assertEqual((len(MODES), len(ORIGINAL)), (18, 4))
        for item in pins()['sources'].values():
            record, cid = item['oracle'], item['registry']['csvid']
            shape = classify(record, cid)
            d = dict(family=FAMILY, version=2, params=shape, oracle_text=record['oracle_text'],
                     variants=variants(shape))
            self.assertEqual(spec(d, record)['csvid'], cid)
            different = deepcopy(record)
            different['oracle_text'] += '\nDraw a card.'
            with self.assertRaises(ValueError):
                spec(d, different)

    def test_upkeep_reveal_and_phasing_order(self):
        for actor in (0, 1):
            s = self.scene(CRUSHER, 'repeat_prefix', actor)
            self.assertEqual([r[4] for r in s.steps if r[0] == 'upkeep_step'], [1-actor, actor] * 2)
            self.assertEqual(s.zones[actor]['library'], ['stop', 'behind'])
            s = self.scene(EQUIPOISE, 'phase_in', actor)
            effects = [r[5] for r in s.steps if r[:4] == ('assert_card', 'land', '', 'in_play')]
            self.assertEqual(effects, [1, 0, 0, 1])

    def test_grave_returns_and_real_costs(self):
        for actor in (0, 1):
            s = self.scene(STINGS, 'repeat', actor)
            self.assertEqual(len([r for r in s.steps if r[:2] == ('cast', 'source')]), 2)
            self.assertEqual(s.life, [22, 18] if actor == 0 else [18, 22])
            i = self.scene(ICHORID, 'return_haste_end', actor)
            self.assertIn('pay', i.zones[actor]['exile'])
            self.assertIn('source', i.zones[actor]['graveyard'])
            self.assertEqual(i.life[1-actor], 17)

    def test_charge_lifecycle(self):
        for actor in (0, 1):
            s = self.scene(BLOOM, 'two_uses', actor)
            self.assertEqual([r[5] for r in s.steps if r[0] == 'choose_color'], [2, 4])
            self.assertEqual(s.zones[actor]['hand'], ['source'])
            c = self.scene(COILS, 'above', actor)
            self.assertEqual(len([r for r in c.steps if r[0] == 'bind_permanent_copy']), 6)
            self.assertEqual(c.zones[actor]['battlefield'], ['source'])

    def test_both_seats_permanent_and_game_receipts(self):
        for actor in (0, 1):
            s = self.scene(SERVITOR, 'copies', actor)
            self.assertEqual([len(z['battlefield']) for z in s.zones.values()], [3, 2] if actor == 0 else [2, 3])
            m = self.scene(MIST, 'other_target', actor)
            self.assertEqual([r[4] for r in m.steps if r[0] == 'choose_target'], [actor] * 2)
            self.assertEqual(self.scene(MORTAL, 'wins', actor).life[1-actor], -99)
            self.assertEqual(self.scene(MORTAL, 'below', actor).life[1-actor], 20)


if __name__ == '__main__':
    unittest.main()
