"""Focused guards for Flash trigger ownership, live return routes and costs."""

from copy import deepcopy
import unittest

import flash_remaining_triggers_and_copy_recipes as model


class RemainingFlashTriggersAndCopyTests(unittest.TestCase):
    def scene(self, csvid, seat, mode):
        pins = model.pins()
        known = pins['sources'] | pins['witnesses']
        rows = {name: data['registry'] for name, data in known.items()}
        oracle = {name: data['oracle'] for name, data in known.items()}
        row, = [r for r in rows.values() if r['csvid'] == csvid]
        scene = model.Scene(row, seat, mode, rows, oracle)
        scene.play()
        return scene

    def test_whole_oracle_is_pinned(self):
        for pin in model.pins()['sources'].values():
            source = pin['oracle']
            cid = pin['registry']['csvid']
            self.assertEqual(model.classify(source, cid)['csvid'], cid)
            wrong = deepcopy(source)
            wrong['oracle_text'] += ' '
            with self.assertRaises(ValueError):
                model.classify(wrong, cid)

    def test_stunt_retains_paid_cast_and_declined_zone_expectations(self):
        for seat in (0, 1):
            scene = self.scene(model.STUNT, seat, 'decline')
            self.assertIn(dict(op='cast_response', ref='source', value=1), scene.steps)
            self.assertIn(dict(op='choose_optional_target', ref='own_candidate', player=seat), scene.steps)
            self.assertIn('Stunt Double', scene.zones[seat]['graveyard'])

    def test_naru_has_owned_copy_and_retarget_expectations(self):
        for seat in (0, 1):
            draw = self.scene(model.NARU, seat, 'own_copy')
            self.assertEqual(draw.zones[seat]['hand'], ['Island', 'Plains'])
            change = self.scene(model.NARU, seat, 'own_retarget')
            self.assertEqual(change.life, [17, 17])
            self.assertIn(dict(op='assert_card', ref='wizard', property='power', value=2), change.steps)

    def test_nebelgast_uses_distinct_real_entries_and_target_intents(self):
        for seat in (0, 1):
            for mode in ('own_spirit', 'foreign_spirit', 'own_nonspirit'):
                scene = self.scene(model.NEBELGAST, seat, mode)
                self.assertEqual([s['ref'] for s in scene.steps if s['op'] == 'choose_target'], ['second', 'ground'])
                self.assertIn(dict(op='assert_card', ref='ground', property='tapped',
                                   value=int(mode == 'own_spirit')), scene.steps)
                self.assertIn(dict(op='assert_card', ref='friend', property='tapped', value=0), scene.steps)

    def test_garna_preserves_real_three_origin_returns(self):
        for seat in (0, 1):
            death = self.scene(model.GARNA, seat, 'battlefield_deaths')
            self.assertEqual(death.zones[seat]['hand'], ['Suntail Hawk', 'Walking Corpse'])
            self.assertIn('Grizzly Bears', death.zones[1 - seat]['graveyard'])
            mill = self.scene(model.GARNA, seat, 'milled')
            self.assertEqual(mill.zones[seat]['hand'], ['Hill Giant'])
            self.assertIn('Lightning Bolt', mill.zones[seat]['graveyard'])
            discarded = self.scene(model.GARNA, seat, 'discarded')
            self.assertEqual(discarded.zones[seat]['hand'], ['Zephyr Sprite', 'Hill Giant'])
            self.assertIn('Ornithopter', discarded.zones[seat]['graveyard'])

    def test_garna_grants_haste_only_to_other_owned_creatures(self):
        for seat in (0, 1):
            scene = self.scene(model.GARNA, seat, 'haste')
            self.assertIn(dict(op='assert_card', ref='source', property='sick', value=1), scene.steps)
            self.assertIn(dict(op='assert_card', ref='parent', property='sick', value=1), scene.steps)
            self.assertIn(dict(op='assert_card', ref='runner', property='sick', value=0), scene.steps)
            self.assertIn(dict(op='attack', ref='runner', value=1), scene.steps)
            self.assertEqual(scene.life[1 - seat], 18)
