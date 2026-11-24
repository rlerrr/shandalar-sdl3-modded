"""Focused checks for exact departures, search inputs, and real return routes."""

from copy import deepcopy
import unittest

from departure_graveyard_and_return_recipes import (
    BARISHI, FAMILY, FOOLS, HULK, LARK, MODES, OIL, ORIGINAL, PHOENIX, RELENTLESS, TOTEM,
    Scene, classify, pins, spec, variants,
)


class DepartureGraveyardAndReturnTest(unittest.TestCase):
    def setUp(self):
        complete = pins()['sources'] | pins()['witnesses']
        self.oracle = {name: pin['oracle'] for name, pin in complete.items()}
        self.rows = {name: pin['registry'] for name, pin in complete.items()}

    def scene(self, csvid, mode, seat):
        row, = (r for r in self.rows.values() if r['csvid'] == csvid)
        result = Scene(row, seat, mode, self.rows, self.oracle)
        result.play()
        return result

    def test_full_records(self):
        self.assertEqual((len(MODES), len(ORIGINAL)), (18, 4))
        for pin in pins()['sources'].values():
            record, cid = pin['oracle'], pin['registry']['csvid']
            shape = classify(record, cid)
            declaration = dict(family=FAMILY, version=2, params=shape, oracle_text=record['oracle_text'],
                               variants=variants(shape))
            self.assertEqual(spec(declaration, record)['csvid'], cid)
            changed = deepcopy(record)
            changed['oracle_text'] += '\nDraw a card.'
            with self.assertRaises(ValueError):
                spec(declaration, changed)

    def test_owner_is_independent_of_trigger_controller(self):
        for seat in (0, 1):
            s = self.scene(BARISHI, 'borrowed_dies', seat)
            self.assertEqual(s.zones[seat]['exile'], ['source'])
            self.assertEqual(s.shuffles[1-seat], 1)
            f = self.scene(FOOLS, 'opp_owner', seat)
            self.assertEqual(f.zones[1-seat]['hand'], ['host'])
            self.assertIn('source', f.zones[seat]['hand'])

    def test_hulk_total_and_zero_and_evoked_source(self):
        for seat in (0, 1):
            h = self.scene(HULK, 'zero_after_cap', seat)
            got = h.zones[seat]['battlefield']
            self.assertEqual(got, ['four', 'two', 'zero'])
            self.assertEqual(sum(h.oracle[h.cards[ref][0]]['cmc'] for ref in got), 6)
            self.assertEqual(h.shuffles[seat], 1)
            l = self.scene(LARK, 'evoke', seat)
            self.assertIn('source', l.zones[seat]['graveyard'])
            self.assertEqual(l.zones[seat]['battlefield'], ['first', 'second'])
            self.assertIn(('choose_dialog', '', '', '', seat, 1, 0, 0, 0), l.steps)

    def test_totem_animation_modes_and_lifecycle(self):
        for seat in (0, 1):
            t = self.scene(TOTEM, 'animated_dies', seat)
            menu = ['Get mana', 'Animate'] if seat == 1 else ['Animate']
            self.assertEqual([r[3] for r in t.steps if r[0] == 'choose_activation_mode'], menu)
            mana_activation = next(i for i, r in enumerate(t.steps) if r[:2] == ('activate', 'source'))
            self.assertIn(('assert_mana', '', '', '', seat, 1, 0, 0, 3), t.steps[mana_activation:])
            self.assertIn('source', t.zones[seat]['hand'])
            u = self.scene(TOTEM, 'unanimated_dies', seat)
            self.assertIn('source', u.zones[seat]['graveyard'])

    def test_both_costs_and_aura_upkeep(self):
        for seat in (0, 1):
            s = self.scene(RELENTLESS, 'both', seat)
            self.assertIn('source', s.zones[seat]['hand'])
            self.assertIn('other_zombie', s.zones[seat]['battlefield'])
            self.assertEqual([x[5] for x in s.steps if x[0] == 'choose_x'], [2] if seat == 0 else [])
            u = self.scene(OIL, 'upkeep', seat)
            self.assertEqual([a[4] for a in u.steps if a[0] == 'upkeep_step'], [1-seat, seat] * 2)
            self.assertIn('source', u.zones[seat]['hand'])
            i = self.scene(OIL, 'infect', seat)
            self.assertIn(('assert_poison', '', '', '', 1-seat, 2, 0, 0, 0), i.steps)

    def test_phoenix_one_return_then_exile(self):
        for seat in (0, 1):
            p = self.scene(PHOENIX, 'two_deaths', seat)
            self.assertIn('source', p.zones[seat]['exile'])
            self.assertEqual([row[:2] for row in p.steps if row[0] == 'bind_permanent'],
                             [('bind_permanent', 'source')])


if __name__ == '__main__':
    unittest.main()
