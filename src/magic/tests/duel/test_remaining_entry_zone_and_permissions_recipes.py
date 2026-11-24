"""Guard paid entry actions, selected zone identities, and real follow-ups."""

from copy import deepcopy
import unittest

from remaining_entry_zone_and_permissions_recipes import (
    ARCANIST, CLOUDFORM, DILUVIAN, FACE_DOWN, FAMILY, JAIL, LIGHTFORM, MODES, ORIGINAL, PLAZA, RAGEFORM,
    SYLVAN, TWILIGHT, VESSEL, WURM, Scene, classify, pins, spec, variants,
)


class RemainingEntryZoneAndPermissionsTest(unittest.TestCase):
    def setUp(self):
        fields = pins()['sources'] | pins()['witnesses']
        self.oracle = {name: row['oracle'] for name, row in fields.items()}
        self.rows = {name: row['registry'] for name, row in fields.items()}

    def scene(self, cid, mode, actor):
        row, = (row for row in self.rows.values() if row['csvid'] == cid)
        result = Scene(row, actor, mode, self.rows, self.oracle)
        result.play()
        return result

    def test_complete_identity_and_known_choices(self):
        self.assertEqual((len(MODES), len(ORIGINAL)), (15, 4))
        for row in pins()['sources'].values():
            record, cid = row['oracle'], row['registry']['csvid']
            shape = classify(record, cid)
            declaration = dict(family=FAMILY, version=2, params=shape, oracle_text=record['oracle_text'],
                               variants=variants(shape))
            self.assertEqual(spec(declaration, record)['csvid'], cid)
            changed = deepcopy(record)
            changed['oracle_text'] += '\nDraw a card.'
            with self.assertRaises(ValueError):
                spec(declaration, changed)

    def test_actual_recast_uses_new_slot_and_does_not_inherit_exile(self):
        for actor in (0, 1):
            s = self.scene(ARCANIST, 'reentry_unlinked', actor)
            first, second = [i for i, row in enumerate(s.steps) if row[0] == 'cast' and row[1] == 'source']
            rebinding, = (i for i, row in enumerate(s.steps) if row[0] == 'bind_hand')
            self.assertLess(first, rebinding)
            self.assertLess(rebinding, second)
            self.assertEqual(s.zones[actor]['exile'], ['instant'])
            self.assertIn(('can_activate', 'source', '', '', 0, 0, 0, 0, 0), s.steps)
            borrowed = self.scene(DILUVIAN, 'draw_and_exile', actor)
            self.assertIn('borrowed_spell', borrowed.zones[1-actor]['exile'])
            self.assertEqual(borrowed.zones[actor]['hand'], ['own_top', 'second_draw'])

    def test_search_qty_zero_and_ordered_bottom(self):
        for actor in (0, 1):
            s = self.scene(SYLVAN, 'indestructible', actor)
            self.assertIn('forest', s.zones[actor]['library'])
            self.assertIn('enemy_permanent', s.zones[1-actor]['battlefield'])
            self.assertEqual(s.shuffles[actor], 1)
            v = self.scene(VESSEL, 'foe_bottom', actor)
            self.assertEqual(v.zones[1-actor]['library'], ['foe_top', 'top_next', 'target'])

    def test_form_modes_use_native_manifest_identity(self):
        for actor in (0, 1):
            for cid in (CLOUDFORM, LIGHTFORM, RAGEFORM):
                s = self.scene(cid, 'face_up', actor)
                self.assertEqual([a[5] for a in s.steps if a[:2] == ('bind_permanent', 'own_top')], [FACE_DOWN])
                checks = [a for a in s.steps if a[0] in (
                    'can_activate_granted_legacy', 'activate_granted_legacy')]
                self.assertEqual([(a[1], a[5], a[7]) for a in checks], [('own_top', 1, cid)] * 2)
                use = s.steps.index(checks[-1])
                phase = next(i for i, a in enumerate(s.steps) if a[0] == 'advance_turn')
                self.assertTrue(all(a[5] == 0 for a in s.steps[use:phase] if a[0] == 'assert_mana'))
                self.assertIn('own_top', s.zones[actor]['battlefield'])
            c = self.scene(CLOUDFORM, 'hexproof', actor)
            self.assertEqual([a[5] for a in c.steps if a[0] == 'can_target_creature'], [0, 1])
            life = self.scene(LIGHTFORM, 'combat', actor).life
            self.assertEqual((life[actor], life[1-actor]), (22, 18))
            self.assertEqual(self.scene(RAGEFORM, 'combat', actor).life[1-actor], 16)

    def test_wurm_control_threshold_and_persist_receipts(self):
        for actor in (0, 1):
            w = self.scene(WURM, 'some', actor)
            self.assertEqual(w.zones[1-actor]['battlefield'], ['artifact', 'creature'])
            self.assertTrue(all(a[4] == 1-actor for a in w.steps if a[0] in ('choose_zone_card', 'choose_optional_zone')))
            t = self.scene(TWILIGHT, 'persist_twice', actor)
            self.assertIn('own_fallen', t.zones[actor]['hand'])
            self.assertIn('source', t.zones[actor]['graveyard'])
            self.assertEqual([a[5] for a in t.steps if a[3] == 'minus1_counters'], [1])
            for mode, life in [('no_own_gates', 20), ('one_gate', 20), ('two_gates', 23)]:
                p = self.scene(PLAZA, mode, actor)
                self.assertEqual(p.life[actor], life)
                self.assertEqual([a[1] for a in p.steps if a[0] == 'play_land'], ['source'])
                self.assertEqual([a[5] for a in p.steps if a[0] == 'choose_color'], [] if mode == 'no_own_gates' else [2])

    def test_dice_model_at_local_exiled_owner_upkeep(self):
        doubles = self.scene(JAIL, 'doubles', 1)
        split = self.scene(JAIL, 'not_doubles', 1)
        self.assertEqual((doubles.seed, split.seed), (2, 1))
        self.assertEqual([a[4] for a in doubles.steps if a[0] == 'upkeep_step'], [0])
        self.assertEqual([a[1] for a in split.steps if a[0] == 'destroy'], ['source'])
        self.assertIn('target', doubles.zones[0]['battlefield'])


if __name__ == '__main__':
    unittest.main()
