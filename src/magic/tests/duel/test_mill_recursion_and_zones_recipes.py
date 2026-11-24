"""Focused checks for paid mill actions, real triggers and owned selections."""

from copy import deepcopy
import unittest

import mill_recursion_and_zones_recipes as model


class MillRecursionAndZonesTests(unittest.TestCase):
    def scene(self, cid, seat, mode):
        pins = model.pins()
        known = pins['sources'] | pins['witnesses']
        oracle = {name: pin['oracle'] for name, pin in known.items()}
        rows = {name: pin['registry'] for name, pin in known.items()}
        row, = [p for p in rows.values() if p['csvid'] == cid]
        scene = model.MillScene(row, seat, mode, rows, oracle)
        scene.play()
        return scene

    def test_exact_whole_oracle(self):
        for pin in model.pins()['sources'].values():
            record, cid = pin['oracle'], pin['registry']['csvid']
            self.assertEqual(model.classify(record, cid)['csvid'], cid)
            changed = deepcopy(record)
            changed['oracle_text'] += ' '
            with self.assertRaises(ValueError):
                model.classify(changed, cid)

    def test_chant_acknowledges_only_real_nonempty_own_upkeeps_in_seat_zero(self):
        for mode, expected in (('creature_upkeep', 1), ('noncreature_upkeep', 1), ('repeat_upkeep', 2)):
            for seat in (0, 1):
                scene = self.scene(model.CHANT, seat, mode)
                acknowledgments = [i for i, step in enumerate(scene.steps) if step['op'] == 'choose_dialog']
                self.assertEqual(len(acknowledgments), expected if seat == 0 else 0)
                for i in acknowledgments:
                    self.assertEqual(scene.steps[i], dict(op='choose_dialog', player=0, value=0))
                    self.assertEqual(scene.steps[i + 1], dict(op='upkeep_step', player=0))
                if mode == 'repeat_upkeep':
                    self.assertEqual([s['player'] for s in scene.steps if s['op'] == 'upkeep_step'],
                                     [1 - seat, seat] * 3)

    def test_false_memories_real_delayed_seven_can_include_older_and_source_cards(self):
        for seat in (0, 1):
            scene = self.scene(model.FALSE_MEMORIES, seat, 'old_and_source')
            end = next(i for i, s in enumerate(scene.steps) if s['op'] == 'cleanup')
            inputs = [s for s in scene.steps[:end] if s['op'] == 'choose_zone_card']
            self.assertEqual(len(inputs), 7)
            self.assertTrue(all(s['player'] == seat for s in inputs))
            self.assertEqual(scene.zones[seat]['exile'][:2], ['Craw Wurm', 'False Memories'])
            self.assertEqual(scene.zones[seat]['graveyard'], ['Zephyr Sprite', 'Island', 'Divination'])

    def test_reclamation_and_grapple_have_distinct_required_selection_policy(self):
        for seat in (0, 1):
            required = self.scene(model.SUDDEN, seat, 'both_milled')
            optional = self.scene(model.GRAPPLE, seat, 'decline')
            self.assertEqual(required.zones[seat]['hand'], ['Grizzly Bears', 'Forest'])
            self.assertIn(dict(op='assert_offered_selection_required', value=1), required.steps)
            self.assertEqual(optional.zones[seat]['hand'], [])
            self.assertIn(dict(op='assert_offered_selection_required', value=0), optional.steps)

    def test_winds_uses_real_control_transfer_and_original_owner(self):
        for seat in (0, 1):
            stolen = self.scene(model.WINDS, seat, 'stolen')
            self.assertIn(dict(op='bind_permanent_copy', ref='borrowed', player=seat, index=0, value=110),
                          stolen.steps)
            self.assertEqual(stolen.zones[1 - seat]['hand'], ['Hill Giant'])
            self.assertEqual(stolen.zones[seat]['library'], ['Mountain'])
            self.assertEqual(stolen.zones[1 - seat]['library'], ['Island'])

    def test_winds_does_not_mill_on_actual_lost_target(self):
        for seat in (0, 1):
            lost = self.scene(model.WINDS, seat, 'target_lost')
            self.assertIn(dict(op='cast_response', ref='bounce', value=1), lost.steps)
            self.assertEqual(lost.zones[seat]['library'], ['Forest', 'Lightning Bolt', 'Mountain'])
            self.assertEqual(lost.zones[1 - seat]['library'], ['Plains', 'Grizzly Bears', 'Island'])
