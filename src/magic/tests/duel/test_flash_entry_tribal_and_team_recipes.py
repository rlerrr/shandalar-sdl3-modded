"""Focused guards for real Flash entry and Spirit of the Hunt distinctions."""

from copy import deepcopy
import unittest

import flash_entry_tribal_and_team_recipes as model


class TribalAndTeamFlashTests(unittest.TestCase):
    def scene(self, csvid, seat, mode):
        data = model.pins()
        source, = [pin['registry'] for pin in data['sources'].values() if pin['registry']['csvid'] == csvid]
        all_pins = data['witnesses'] | data['sources']
        rows = {name: pin['registry'] for name, pin in all_pins.items()}
        oracle = {name: pin['oracle'] for name, pin in all_pins.items()}
        scene = model.Scene(source, seat, mode, rows, oracle)
        scene.play()
        return scene

    def test_exact_whole_oracle_is_required(self):
        for pin in model.pins()['sources'].values():
            record, csvid = pin['oracle'], pin['registry']['csvid']
            self.assertEqual(model.classify(record, csvid)['csvid'], csvid)
            changed = deepcopy(record)
            changed['oracle_text'] += ' '
            with self.assertRaises(ValueError):
                model.classify(changed, csvid)

    def test_hunt_always_flashes_over_a_real_other_turn_spell(self):
        for seat in (0, 1):
            for mode in model.MODES[model.HUNT]:
                scene = self.scene(model.HUNT, seat, mode)
                self.assertEqual(scene.steps[0], dict(op='turn', player=1 - seat))
                events = [s for s in scene.steps if s['op'] in ('announce_spell', 'cast_response', 'resolve_response')]
                self.assertEqual(events, [dict(op='announce_spell', ref='parent'),
                                          dict(op='cast_response', ref='source', value=1),
                                          dict(op='resolve_response')])

    def test_wolf_werewolf_and_both_types_each_get_exactly_three_toughness(self):
        def values(scene, ref, prop):
            return [s['value'] for s in scene.steps if s.get('ref') == ref and s.get('property') == prop]

        for seat in (0, 1):
            scene = self.scene(model.HUNT, seat, 'mixed_team')
            self.assertEqual(values(scene, 'wolf', 'toughness'), [5, 2])
            self.assertEqual(values(scene, 'werewolf', 'toughness'), [7, 4])
            self.assertEqual(values(scene, 'changeling', 'toughness'), [4, 1])
            self.assertEqual(values(scene, 'werewolf', 'tapped'), [1])
            self.assertEqual(values(scene, 'own_foil', 'toughness'), [2, 2])
            self.assertEqual(values(scene, 'foe_wolf', 'toughness'), [2, 2])
            self.assertEqual(values(scene, 'foe_werewolf', 'toughness'), [4, 4])
            self.assertTrue(all(toughness == 3 for toughness in values(scene, 'source', 'toughness')))

    def test_hunt_saves_each_pure_subtype_in_actual_combat(self):
        for seat in (0, 1):
            for mode, blocker, damage in (('combat_wolf', 'wolf', 3), ('combat_werewolf', 'werewolf', 6)):
                scene = self.scene(model.HUNT, seat, mode)
                self.assertIn(dict(op='block', ref=blocker, other='foe_attacker', value=1), scene.steps)
                damage_step = scene.steps.index(dict(op='combat_damage'))
                cleanup = scene.steps.index(dict(op='cleanup'))
                self.assertIn(dict(op='assert_card', ref=blocker, property='damage', value=damage),
                              scene.steps[damage_step:cleanup])
                self.assertIn(dict(op='assert_card', ref=blocker, property='damage', value=0),
                              scene.steps[cleanup:])

    def test_only_source_excludes_self_even_when_foe_has_each_type(self):
        for seat in (0, 1):
            scene = self.scene(model.HUNT, seat, 'only_source')
            self.assertEqual(scene.zones[seat]['battlefield'], ['Grizzly Bears', 'Spirit of the Hunt'])
            self.assertTrue(all(s['value'] == 3 for s in scene.steps
                                if s.get('ref') == 'source' and s.get('property') == 'toughness'))
