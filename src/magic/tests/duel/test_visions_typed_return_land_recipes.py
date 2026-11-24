"""Focused guards for land actions, optional return, ownership, and mana."""

import json
import unittest

from generate_cases import registry
import visions_typed_return_land_recipes as model


class VisionsTypedReturnLandTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle = json.loads((model.PROFILE_PATH.parent / 'oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {row['name']: row for row in registry()}

    def case(self, name, mode):
        record = self.oracle[name]
        declaration = dict(family=model.FAMILY, version=2, oracle_text=record['oracle_text'],
                           params=model.parameters(record), variants=[mode])
        return model.expand(name, self.rows[name]['csvid'], declaration, model.spec(declaration, record),
                            [mode], self.rows, self.oracle)[0]

    @staticmethod
    def values(steps, op, **fields):
        return [step['value'] for step in steps if step['op'] == op and all(step.get(key) == value for key, value in fields.items())]

    def test_exact_full_oracle_and_paired_colors(self):
        model.validate_witnesses(self.oracle, self.rows)
        expected = {2683: ('Island', 'U'), 2691: ('Mountain', 'R'), 2700: ('Swamp', 'B'),
                    2728: ('Forest', 'G'), 2730: ('Plains', 'W')}
        self.assertEqual({pin['registry']['csvid'] for pin in model.profiles()['sources'].values()}, set(expected))
        for name, pin in model.profiles()['sources'].items():
            record = self.oracle[name]
            subtype, color = expected[pin['registry']['csvid']]
            self.assertEqual(model.parameters(record), dict(subtype=subtype, output=['C', color], entry_tapped=True,
                                                            return_to='owner', sacrifice_on_decline=True))
            for bad in (record['oracle_text'].replace('untapped', 'basic'), record['oracle_text'].replace('owner', 'controller'),
                        record['oracle_text'].replace('{C}', '{1}'), record['oracle_text'] + '\nFlying'):
                with self.assertRaises(ValueError):
                    model.parameters(dict(record, oracle_text=bad))

    def test_normal_land_play_consumes_single_allowance_even_after_sacrifice(self):
        for owner in (0, 1):
            for mode in ('play_matching', 'play_nonbasic', 'play_decline', 'play_no_land', 'play_sacrifice_uses_allowance'):
                steps = self.case('Coral Atoll', f'{mode}_{owner}')['steps']
                self.assertEqual(self.values(steps, 'play_land', ref='source'), [1])
                self.assertFalse(any(step['op'] in ('cast', 'cast_response', 'resolve', 'activate') for step in steps))
                self.assertEqual(self.values(steps, 'assert_card', ref='source', property='in_play')[-1], int(mode in ('play_matching', 'play_nonbasic')))
            steps = self.case('Coral Atoll', f'play_sacrifice_uses_allowance_{owner}')['steps']
            self.assertEqual(self.values(steps, 'play_land', ref='next_land'), [0])
            self.assertEqual(self.values(steps, 'assert_zone', player=owner, zone='graveyard')[-1], 2)
            steps = self.case('Coral Atoll', f'play_after_land_{owner}')['steps']
            self.assertEqual(self.values(steps, 'play_land', ref='source'), [0, 1])
            self.assertEqual([step['player'] for step in steps if step['op'] == 'untap_step'], [1 - owner, owner])

    def test_ordinary_and_atlas_permissions_make_distinct_real_actions(self):
        steps = self.case('Coral Atoll', 'play_offturn_denied_1')['steps']
        self.assertIn(dict(op='turn', player=0, value=0), steps)
        self.assertEqual(self.values(steps, 'play_land', ref='source'), [0])
        steps = self.case('Coral Atoll', 'atlas_pending_then_play_denied_1')['steps']
        cast = steps.index(dict(op='play_land', ref='source', value=0))
        self.assertEqual(self.values(steps[:cast], 'assert_stack')[-1], 1)
        self.assertEqual(self.values(steps[cast:], 'assert_card', ref='source', property='in_play')[-1], 1)
        steps = self.case('Coral Atoll', 'atlas_leaves_play_available_1')['steps']
        self.assertLess(steps.index(dict(op='resolve')), steps.index(dict(op='play_land', ref='match', value=1)))
        self.assertIn(dict(op='assert_card', ref='match', property='in_play', value=1), steps)

    def test_untapped_subtype_and_control_requirements_are_independent(self):
        for owner in (0, 1):
            for mode in ('no_matching', 'wrong_subtype', 'tapped_matching', 'foreign_controller', 'owned_wrong_control', 'matching_in_hand'):
                steps = self.case('Everglades', f'{mode}_{owner}')['steps']
                self.assertEqual(self.values(steps, 'assert_card', ref='source', property='in_play')[-1], 0)
                self.assertNotIn('choose_target', [step['op'] for step in steps])
                self.assertEqual(self.values(steps, 'assert_zone', player=owner, zone='graveyard')[-1], 2)
            case = self.case('Everglades', f'matching_nonbasic_{owner}')
            self.assertIn(dict(name='Bayou', ref='match', player=owner, zone='battlefield'), case['setup'])
            self.assertIn(dict(op='choose_target', player=owner, ref='match'), case['steps'])
        steps = self.case('Everglades', 'tap_during_atlas_1')['steps']
        choice = steps.index(dict(op='choose_zone_card', player=1, value=2700))
        self.assertIn(dict(op='activate', ref='match', value=1), steps[:choice])
        self.assertEqual(self.values(steps, 'assert_mana', player=1, color=1)[-1], 1)
        self.assertEqual(self.values(steps, 'assert_card', ref='source', property='in_play')[-1], 0)

    def test_actual_decline_and_foreign_owner_return(self):
        for owner in (0, 1):
            steps = self.case('Jungle Basin', f'decline_{owner}')['steps']
            self.assertIn(dict(op='choose_cancel_target', player=owner), steps)
            self.assertEqual(self.values(steps, 'assert_card', ref='match', property='in_play')[-1], 1)
            self.assertEqual(self.values(steps, 'assert_zone', player=owner, zone='graveyard')[-1], 2)
            case = self.case('Jungle Basin', f'foreign_owner_{owner}')
            land = next(item for item in case['setup'] if item['ref'] == 'match')
            self.assertEqual(land['state'], ['opponent_owned'])
            self.assertIn(dict(op='bind_hand', player=1 - owner, value=self.rows[land['name']]['csvid'], ref='match'), case['steps'])
        protected = self.case('Jungle Basin', 'indestructible_sacrifice_0')
        self.assertIn(dict(name='Terra Eternal', ref='protection', player=0, zone='battlefield'), protected['setup'])
        self.assertEqual(self.values(protected['steps'], 'assert_zone', player=0, zone='graveyard')[-1], 2)

    def test_tapped_entry_then_both_actual_mana_and_exact_spend(self):
        denied = self.case('Karoo', 'entry_tapped_denied_0')['steps']
        self.assertEqual(self.values(denied, 'activate', ref='source'), [0])
        self.assertEqual(self.values(denied, 'assert_mana_total', player=0)[-1], 0)
        for name, color in [('Coral Atoll', 2), ('Dormant Volcano', 4), ('Everglades', 1), ('Jungle Basin', 3), ('Karoo', 5)]:
            steps = self.case(name, 'spend_pair_1')['steps']
            cast = steps.index(dict(op='cast', ref='spender', value=1))
            self.assertEqual(self.values(steps[:cast], 'activate', ref='source'), [1])
            self.assertEqual(self.values(steps[:cast], 'assert_mana', player=1, color=0)[-1], 1)
            self.assertEqual(self.values(steps[:cast], 'assert_mana', player=1, color=color)[-1], 1)
            self.assertEqual(self.values(steps[cast:], 'assert_mana_total', player=1)[-1], 0)
            self.assertFalse(any(step['op'] in ('choose_color', 'choose_dialog') for step in steps))

    def test_replay_and_reentry_replay_real_boundaries(self):
        for mode, op, ref, count in [('play_replay', 'play_land', 'source', 2), ('pending_replay', 'activate', 'atlas', 1),
                                     ('produce_replay', 'activate', 'source', 2)]:
            steps = self.case('Dormant Volcano', mode + '_1')['steps']
            self.assertEqual(self.values(steps, op, ref=ref), [1] * count)
            self.assertIn(dict(op='assert_recorded_state'), steps)
        steps = self.case('Dormant Volcano', 'reenter_0')['steps']
        self.assertIn(dict(op='cast', ref='bounce', value=1), steps)
        self.assertIn(dict(op='choose_target', player=0, ref='second_match'), steps)
        self.assertEqual(self.values(steps, 'activate', ref='atlas'), [1, 1])


if __name__ == '__main__':
    unittest.main()
