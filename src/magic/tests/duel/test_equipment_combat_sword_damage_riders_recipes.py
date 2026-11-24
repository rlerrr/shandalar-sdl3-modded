"""Focused whole-record, payment and hand-count checks for combat Swords."""

from copy import deepcopy
import unittest

import equipment_combat_sword_damage_riders_recipes as model


class SwordDamageRidersTests(unittest.TestCase):
    def scene(self, csvid, seat, mode):
        data = model.pins()
        source, = [pin['registry'] for pin in data['sources'].values() if pin['registry']['csvid'] == csvid]
        all_pins = data['witnesses'] | data['sources']
        rows = {name: pin['registry'] for name, pin in all_pins.items()}
        oracle = {name: pin['oracle'] for name, pin in all_pins.items()}
        scene = model.SwordScene(source, mode, seat, rows, oracle)
        scene.play()
        return scene

    def test_sources_require_exact_whole_oracle(self):
        for pin in model.pins()['sources'].values():
            record, csvid = pin['oracle'], pin['registry']['csvid']
            self.assertEqual(model.classify(record, csvid)['equip'], [2, 0, 0, 0, 0, 0])
            changed = deepcopy(record)
            changed['oracle_text'] += ' '
            with self.assertRaises(ValueError):
                model.classify(changed, csvid)

    def test_paid_cast_equip_and_protection_are_live(self):
        for csvid in model.MODES:
            for seat in (0, 1):
                scene = self.scene(csvid, seat, 'protected_blocks')
                for op, value in (('cast', 3), ('activate', 2)):
                    action = next(i for i, step in enumerate(scene.steps) if step['op'] == op)
                    funding = [s for s in scene.steps[:action]
                               if s['op'] == 'set_mana' and s['player'] == seat and s['color'] == 0]
                    self.assertEqual(funding[-1]['value'], value)
                self.assertEqual([(s['ref'], s['value']) for s in scene.steps if s['op'] == 'block'],
                                 [('protected_0', 0), ('protected_1', 0)])

    def test_war_empty_hand_riders_are_independent(self):
        cases = {'connect': (22, 12), 'owner_empty': (20, 12), 'victim_empty': (22, 15)}
        for seat in (0, 1):
            for mode, (owner, victim) in cases.items():
                scene = self.scene(model.WAR, seat, mode)
                self.assertEqual((scene.life[seat], scene.life[1 - seat]), (owner, victim))

    def test_war_changed_hands_use_real_response_and_draw(self):
        for seat in (0, 1):
            scene = self.scene(model.WAR, seat, 'changed_hands')
            self.assertIn(dict(op='announce_spell', ref='kept_1'), scene.steps)
            self.assertIn(dict(op='cast_response', ref='opposing_1', value=1), scene.steps)
            self.assertIn(dict(op='resolve_response'), scene.steps)
            self.assertEqual(scene.zones[seat]['hand'], ['Swamp', 'Plains', 'Forest'])
            self.assertEqual(scene.zones[1 - seat]['hand'], ['Mountain'])
            self.assertEqual((scene.life[seat], scene.life[1 - seat]), (23, 14))

    def test_war_nonplayer_and_noncombat_controls_preserve_riders(self):
        for seat in (0, 1):
            for mode, damage in (('legal_block', 0), ('noncombat', 1)):
                scene = self.scene(model.WAR, seat, mode)
                self.assertEqual((scene.life[seat], scene.life[1 - seat]), (20, 20 - damage))
                self.assertEqual(len(scene.zones[seat]['hand']), 2)
                self.assertEqual(len(scene.zones[1 - seat]['hand']), 3)

    def test_light_graveyard_shroud_preserves_life_rider(self):
        for seat in (0, 1):
            scene = self.scene(model.LIGHT, seat, 'ground_seal')
            self.assertEqual(scene.life[seat], 23)
            self.assertEqual(scene.zones[seat]['hand'], ['Plains'])
            self.assertEqual(scene.zones[seat]['graveyard'], ['Lightning Bolt', 'Hill Giant'])
            self.assertFalse(any(s['op'] in ('choose_zone_card', 'choose_optional_zone') for s in scene.steps))
