"""Guards for exact Staff clauses and independently observable trigger boundaries."""

import json
import unittest

from generate_cases import registry
import staff_magus_cast_land_life_recipes as model


class StaffMagusCastLandLifeTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle = json.loads((model.PROFILE_PATH.parent / 'oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {row['name']: row for row in registry()}

    def case(self, name, variant):
        name = f'Staff of the {name} Magus'
        record = self.oracle[name]
        declaration = dict(family=model.FAMILY, version=2, oracle_text=record['oracle_text'],
                           params=model.parameters(record), variants=[variant])
        params = model.spec(declaration, record)
        return model.expand(name, self.rows[name]['csvid'], declaration, params, [variant], self.rows, self.oracle)[0]

    @staticmethod
    def values(steps, op, **fields):
        return [step['value'] for step in steps if step['op'] == op and all(step.get(key) == value for key, value in fields.items())]

    def test_full_pinned_clauses_and_exact_color_subtype_pairs(self):
        model.validate_witnesses(self.oracle, self.rows)
        expected = {13340: ('B', 'Swamp'), 13341: ('R', 'Mountain'), 13342: ('U', 'Island'),
                    13343: ('W', 'Plains'), 13344: ('G', 'Forest')}
        pins = model.profiles()['sources']
        self.assertEqual(len(pins), len(expected))
        for name, pin in pins.items():
            record = self.oracle[name]
            params = model.parameters(record)
            self.assertEqual((params['color'], params['subtype']), expected[pin['registry']['csvid']])
            self.assertEqual(params['printed_mana'], '{3}')
            self.assertEqual(params['gain'], 1)
            for text in (record['oracle_text'].replace(params['subtype'], 'Desert'), record['oracle_text'] + '\nFlying'):
                with self.assertRaises(ValueError):
                    model.parameters(dict(record, oracle_text=text))

    def test_own_spell_gain_precedes_its_independent_resolution_effect(self):
        case = self.case('Sun', 'match_spell_0')
        self.assertIn(dict(name="Whitesun's Passage", ref='spell', player=0, zone='hand'), case['setup'])
        steps = case['steps']
        announced, resolved = (steps.index(dict(op='announce_spell', ref='spell')), steps.index(dict(op='resolve')))
        self.assertEqual(self.values(steps[:announced], 'assert_life', player=0)[-1], 20)
        self.assertEqual(self.values(steps[announced:resolved], 'assert_life', player=0), [21])
        self.assertEqual(self.values(steps[resolved:], 'assert_life', player=0)[-1], 26)
        for variant, life in [('wrong_spell_0', 20), ('colorless_spell_0', 20), ('multicolored_spell_0', 21),
                              ('two_sources_0', 27), ('two_matching_spells_0', 32), ('tapped_staff_0', 26)]:
            with self.subTest(variant=variant):
                self.assertEqual(self.values(self.case('Sun', variant)['steps'], 'assert_life', player=0)[-1], life)

    def test_opponent_cast_and_counter_leave_correct_life_and_real_cards(self):
        steps = self.case('Sun', 'opponent_spell_1')['steps']
        self.assertIn(dict(op='turn', player=0, value=0), steps)
        self.assertEqual(self.values(steps, 'assert_life', player=0)[-1], 25)
        self.assertEqual(self.values(steps, 'assert_life', player=1)[-1], 20)
        steps = self.case('Sun', 'countered_matching_spell_1')['steps']
        announced = steps.index(dict(op='announce_spell', ref='spell'))
        countered = steps.index(dict(op='cast_response', ref='counter', value=1))
        self.assertLess(announced, countered)
        self.assertEqual(self.values(steps[announced:countered], 'assert_life', player=1), [21])
        self.assertEqual(self.values(steps, 'assert_life', player=1)[-1], 21)
        self.assertEqual(self.values(steps, 'assert_zone', player=1, zone='graveyard')[-1], 2)
        self.assertEqual(self.values(steps, 'assert_zone', player=0, zone='graveyard')[-1], 2)

    def test_twincast_is_two_casts_and_two_draws_without_third_life(self):
        for player in (0, 1):
            steps = self.case('Mind', f'copy_does_not_trigger_{player}')['steps']
            self.assertEqual([s['ref'] for s in steps if s['op'] == 'announce_spell'], ['spell', 'copy_spell'])
            self.assertIn(dict(op='cast_response', ref='copy_spell', value=1), steps)
            self.assertIn(dict(op='resolve_response'), steps)
            self.assertEqual(self.values(steps, 'assert_life', player=player)[-1], 22)
            self.assertEqual(self.values(steps, 'assert_zone', player=player, zone='hand')[-1], 4)
            self.assertEqual(self.values(steps, 'assert_zone', player=player, zone='library')[-1], 5)
            self.assertEqual(self.values(steps, 'assert_zone', player=player, zone='graveyard')[-1], 3)

    def test_atlas_entry_observes_controller_dual_subtype_and_non_cast_land(self):
        for variant, actor, life in [('land_own_0', 0, 21), ('land_enemy_0', 1, 20), ('land_wrong_subtype_0', 0, 20),
                                     ('land_dual_0', 0, 21), ('colored_land_creature_0', 0, 21)]:
            with self.subTest(variant=variant):
                case = self.case('Wild', variant)
                steps = case['steps']
                land = next(item for item in case['setup'] if item['ref'] == 'land')
                self.assertIn(dict(op='activate', ref='atlas', value=1), steps)
                self.assertIn(dict(op='choose_zone_card', player=actor, value=self.rows[land['name']]['csvid']), steps)
                self.assertEqual(self.values(steps, 'assert_card', ref='land', property='in_hand'), [1, 0])
                self.assertEqual(sum(item['name'] == land['name'] and item['zone'] == 'hand' and item['player'] == actor
                                     for item in case['setup']), 1)
                self.assertEqual(self.values(steps, 'assert_life', player=0)[-1], life)
                self.assertEqual(self.values(steps, 'assert_life', player=1)[-1], 20)
                self.assertFalse(any(s['op'] in ('cast', 'cast_response', 'announce_spell') for s in steps))
        case = self.case('Wild', 'setup_land_1')
        refs = [item['ref'] for item in case['setup']]
        self.assertLess(refs.index('land'), refs.index('source'))
        self.assertEqual(self.values(case['steps'], 'assert_life', player=1)[-1], 20)

    def test_search_and_two_events_keep_cast_and_entry_distinct(self):
        for name, after_cast, after_entry in [('Wild', 21, 22), ('Death', 20, 21)]:
            steps = self.case(name, 'search_basic_land_1')['steps']
            cut = steps.index(dict(op='resolve'))
            self.assertEqual(self.values(steps[:cut], 'assert_life', player=1)[-1], after_cast)
            self.assertEqual(self.values(steps[cut:], 'assert_life', player=1)[-1], after_entry)
            self.assertIn(dict(op='assert_card', ref='searched_land', property='tapped', value=1), steps)
            self.assertEqual(self.values(steps, 'assert_zone', player=1, zone='library')[-1], 0)
        steps = self.case('Wild', 'two_events_1')['steps']
        self.assertEqual(self.values(steps, 'assert_life', player=1)[-1], 22)
        self.assertIn(dict(op='announce_spell', ref='spell'), steps)
        self.assertIn(dict(op='activate', ref='atlas', value=1), steps)

    def test_snapshot_boundaries_repeat_only_real_pending_actions(self):
        for variant, announcements, activations in [('announce_replay_1', 2, 0), ('announced_replay_1', 1, 0),
                                                     ('land_pending_replay_1', 0, 1)]:
            steps = self.case('Mind', variant)['steps']
            ops = [step['op'] for step in steps]
            self.assertEqual(ops.count('announce_spell'), announcements)
            self.assertEqual(ops.count('activate'), activations)
            self.assertEqual(ops.count('resolve'), 2)
            self.assertEqual(ops.count('choose_zone_card'), 2 * activations)
            cut = ops.index('capture')
            self.assertEqual(ops.index('assert_recorded_state'), len(ops) - 1)
            self.assertNotIn('cast', ops[cut:])
            self.assertNotIn('activate', ops[cut:])
            self.assertEqual(self.values(steps, 'assert_life', player=1)[-1], 21)


if __name__ == '__main__':
    unittest.main()
