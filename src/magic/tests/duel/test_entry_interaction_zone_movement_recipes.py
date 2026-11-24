"""Guard exact entry interactions and their independent typed witnesses."""

from copy import deepcopy
import unittest

from entry_interaction_zone_movement_recipes import (
    DAY, FAITH, FAMILY, HARBINGER, MODES, ORIGINAL, OUBLIETTE, PRISON, QUARANTINE, QUARRY,
    Scene, classify, pins, spec, variants,
)


class EntryInteractionZoneMovementTest(unittest.TestCase):
    def setUp(self):
        fields = pins()['sources'] | pins()['witnesses']
        self.oracle = {name: r['oracle'] for name, r in fields.items()}
        self.rows = {name: r['registry'] for name, r in fields.items()}

    def case(self, cid, mode, actor):
        row, = (r for r in self.rows.values() if r['csvid'] == cid)
        result = Scene(row, actor, mode, self.rows, self.oracle)
        result.play()
        return result

    def test_complete_exact_sources(self):
        self.assertEqual((len(MODES), len(ORIGINAL)), (16, 4))
        for entry in pins()['sources'].values():
            record, cid = entry['oracle'], entry['registry']['csvid']
            shape = classify(record, cid)
            declaration = dict(family=FAMILY, version=2, params=shape, oracle_text=record['oracle_text'],
                               variants=variants(shape))
            self.assertEqual(spec(declaration, record)['csvid'], cid)
            modified = deepcopy(record)
            modified['oracle_text'] += '\nDraw a card.'
            with self.assertRaises(ValueError):
                spec(declaration, modified)

    def test_grave_return_and_nonowner_control(self):
        for actor in (0, 1):
            p = self.case(PRISON, 'opponent_pays', actor)
            self.assertEqual([item[4] for item in p.steps if item[0] == 'choose_dialog'], [1-actor])
            self.assertIn('target', p.zones[1-actor]['battlefield'])
            d = self.case(DAY, 'borrowed_owner', actor)
            self.assertIn('first', d.zones[1-actor]['hand'])
            self.assertIn('owner_probe', d.zones[actor]['graveyard'])
            self.assertEqual([a[1] for a in d.steps if a[0] == 'cast'],
                             ['theft', 'source', 'remover', 'owner_probe'])

    def test_phasing_stays_in_the_same_slots(self):
        for actor in (0, 1):
            s = self.case(OUBLIETTE, 'attachments', actor)
            self.assertEqual([a[5] for a in s.steps if a[:4] == ('assert_card', 'aura', '', 'in_play')], [0, 1])
            self.assertEqual(s.zones[actor]['battlefield'], ['aura'])
            self.assertEqual(s.zones[1-actor]['battlefield'], ['target', 'equipment'])

    def test_variable_cost_ceilings_and_actual_flash(self):
        for actor in (0, 1):
            q = self.case(QUARANTINE, 'up_to_one', actor)
            self.assertEqual([a[5] for a in q.steps if a[0] == 'choose_x'], [4])
            self.assertEqual([a[4] for a in q.steps if a[0] == 'choose_done_target'], [actor])
            h = self.case(HARBINGER, 'flash', actor)
            self.assertEqual([a[4] for a in h.steps if a[0] == 'turn'], [actor, 1-actor])
            self.assertIn('target', h.zones[1-actor]['hand'])
            denied = self.case(HARBINGER, 'flash_short', actor).steps
            self.assertFalse(any(row[0] == 'can_cast' for row in denied))
            failed, = (i for i, row in enumerate(denied) if row[0] == 'cast')
            self.assertEqual(denied[failed][1:6], ('source', '', '', 0, 0))
            self.assertEqual(denied[failed+1][1:6], ('source', '', 'in_hand', 0, 1))

    def test_owner_library_order_and_failed_due_to_lost_host(self):
        for actor in (0, 1):
            q = self.case(QUARRY, 'borrowed_owner', actor)
            self.assertEqual(q.zones[1-actor]['library'], ['foe_top', 'target', 'next_card'])
            f = self.case(FAITH, 'lost_host', actor)
            self.assertIn('host', f.zones[actor]['hand'])
            self.assertEqual(f.zones[actor]['graveyard'], ['source'])
            self.assertEqual([a[0] for a in f.steps].count('cast_response'), 1)


if __name__ == '__main__':
    unittest.main()
