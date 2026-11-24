"""Focused checks for real payment, owner return, and exact mana output."""

import json
import unittest

from generate_cases import registry
import borderpost_alternate_cast_recipes as model


class BorderpostAlternateCastTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle = json.loads((model.PROFILE_PATH.parent / 'oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {row['name']: row for row in registry()}

    def case(self, kind, mode):
        name = f'{kind} Borderpost'
        record = self.oracle[name]
        declaration = dict(family=model.FAMILY, version=2, oracle_text=record['oracle_text'],
                           params=model.parameters(record), variants=[mode])
        return model.expand(name, self.rows[name]['csvid'], declaration, model.spec(declaration, record),
                            [mode], self.rows, self.oracle)[0]

    @staticmethod
    def values(steps, op, **fields):
        return [step['value'] for step in steps if step['op'] == op and all(step.get(key) == value for key, value in fields.items())]

    def test_whole_clause_and_printed_colors_are_pinned(self):
        model.validate_witnesses(self.oracle, self.rows)
        colors = {10273: ['W', 'U'], 10277: ['R', 'G'], 10318: ['U', 'B'], 10370: ['B', 'R'], 10376: ['G', 'W']}
        sources = model.profiles()['sources']
        self.assertEqual(len(sources), len(colors))
        for name, pin in sources.items():
            record = self.oracle[name]
            params = model.parameters(record)
            self.assertEqual(params['colors'], colors[pin['registry']['csvid']])
            self.assertEqual(params['return_to'], 'owner')
            self.assertEqual(params['alternate_generic'], 1)
            self.assertTrue(params['entry_tapped'])
            for text in (record['oracle_text'].replace('basic land', 'land'),
                         record['oracle_text'].replace('owner', 'controller'), record['oracle_text'].replace('tapped', 'untapped')):
                with self.assertRaises(ValueError):
                    model.parameters(dict(record, oracle_text=text))

    def test_prompt_choices_only_with_both_payment_paths(self):
        entries = [('normal_empty', [], 1, 0), ('normal_with_basic', [0], 1, 0), ('alternate_only', [], 1, 1),
                   ('alternate_menu', [1], 1, 1), ('cancel_menu', [2], 0, 0)]
        for owner in (0, 1):
            for variant, menu, allowed, returned in entries:
                with self.subTest(owner=owner, variant=variant):
                    steps = self.case('Fieldmist', f'{variant}_{owner}')['steps']
                    self.assertEqual(self.values(steps, 'choose_dialog', player=owner), menu)
                    self.assertEqual(self.values(steps, 'cast', ref='source'), [allowed])
                    self.assertEqual(sum(step['op'] == 'bind_hand' for step in steps), returned)
                    self.assertEqual(sum(step['op'] == 'choose_target' for step in steps), returned * (owner == 0))
                    balance = 3 if variant == 'cancel_menu' else 2 if variant == 'alternate_menu' else 0
                    self.assertEqual(self.values(steps, 'assert_mana_total', player=owner)[-1], balance)

    def test_control_and_ownership_are_distinct_and_return_precedes_resolution(self):
        for controller in (0, 1):
            case = self.case('Firewild', f'alternate_foreign_owner_{controller}')
            land = next(item for item in case['setup'] if item['ref'] == 'land')
            self.assertEqual(land, dict(name='Snow-Covered Island', ref='land', player=controller,
                                        zone='battlefield', state=['tapped', 'opponent_owned']))
            steps = case['steps']
            cut = steps.index(dict(op='resolve'))
            csvid = self.rows[land['name']]['csvid']
            self.assertIn(dict(op='bind_hand', ref='returned_land', player=1 - controller, value=csvid), steps[:cut])
            self.assertIn(dict(op='assert_zone_multiplicity', player=controller, zone='hand', index=csvid, value=0), steps[:cut])
            self.assertIn(dict(op='assert_card', ref='source', property='in_play', value=0), steps[:cut])
            steps = self.case('Firewild', f'foreign_controller_only_{controller}')['steps']
            self.assertIn(dict(op='cast', ref='source', value=0), steps)
            self.assertFalse(any(step['op'] == 'bind_hand' for step in steps))

    def test_legal_basic_choice_and_mana_from_same_land(self):
        human = self.case('Mistvein', 'choose_second_basic_0')['steps']
        self.assertIn(dict(op='choose_target', player=0, ref='second_land'), human)
        self.assertIn(dict(op='assert_card', ref='land', property='in_play', value=1), human)
        for owner in (0, 1):
            steps = self.case('Mistvein', f'alternate_spent_land_{owner}')['steps']
            tap = steps.index(dict(op='activate', ref='land', value=1))
            cast = steps.index(dict(op='cast', ref='source', value=1))
            self.assertEqual(self.values(steps[tap:cast], 'assert_mana', player=owner, color=3), [1])
            self.assertEqual(self.values(steps[cast:], 'assert_mana', player=owner, color=3)[-1], 0)
            manual = [dict(op='choose_mana', player=0, color=3), dict(op='choose_target', player=0, ref='land')]
            self.assertEqual([step for step in steps[tap:cast] if step['op'].startswith('choose_')], manual if owner == 0 else [])
            floating = self.case('Veinfire', f'alternate_color_float_{owner}')['steps']
            choices = [step for step in floating if step['op'].startswith('choose_')]
            self.assertEqual(choices, [dict(op='choose_mana', player=0, color=2), manual[1]] if owner == 0 else [])
            for mode in ('normal_wrong_colors', 'normal_no_generic', 'nonbasic_only', 'no_payment', 'land_in_hand'):
                denied = self.case('Mistvein', f'{mode}_{owner}')['steps']
                self.assertIn(dict(op='cast', ref='source', value=0), denied)
                self.assertFalse(any(step['op'] in ('bind_hand', 'choose_target') for step in denied))

    def test_tapped_entry_and_both_outputs_come_from_real_mana_activations(self):
        denied = self.case('Veinfire', 'entry_tapped_denied_0')['steps']
        cut = denied.index(dict(op='activate', ref='source', value=0))
        self.assertIn(dict(op='assert_card', ref='source', property='tapped', value=1), denied[:cut])
        self.assertFalse(any(step['op'] == 'untap_step' for step in denied))
        twice = self.case('Veinfire', 'mana_twice_1')['steps']
        self.assertEqual(self.values(twice, 'choose_color', player=1), [1, 4])
        self.assertEqual(self.values(twice, 'assert_mana', player=1, color=1)[-1], 1)
        self.assertEqual(self.values(twice, 'assert_mana', player=1, color=4)[-1], 1)
        self.assertEqual(self.values(twice, 'assert_mana_total', player=1)[-1], 2)
        self.assertIn(dict(op='cast', ref='untap', value=1), twice)
        offturn = self.case('Veinfire', 'mana_offturn_1')['steps']
        self.assertEqual([step['player'] for step in offturn if step['op'] == 'untap_step'], [0, 1, 0])
        spent = self.case('Veinfire', 'spend_second_1')['steps']
        self.assertIn(dict(op='cast', ref='spender', value=1), spent)
        self.assertEqual(self.values(spent, 'assert_mana', player=1, color=4)[-1], 0)

    def test_countered_spell_keeps_prior_land_payment_and_snapshot_replays_boundary(self):
        steps = self.case('Wildfield', 'alternate_countered_1')['steps']
        counter = steps.index(dict(op='cast_response', ref='counter', value=1))
        self.assertTrue(any(step['op'] == 'bind_hand' for step in steps[:counter]))
        self.assertEqual(self.values(steps, 'assert_card', ref='source', property='in_play')[-1], 0)
        self.assertEqual(self.values(steps, 'assert_zone', player=1, zone='graveyard')[-1], 2)
        for mode, cast_count in (('prior_cast_snapshot', 2), ('pending_cost_snapshot', 1)):
            steps = self.case('Wildfield', f'{mode}_1')['steps']
            self.assertEqual(self.values(steps, 'cast', ref='source'), [1] * cast_count)
            self.assertEqual(sum(step['op'] == 'bind_hand' for step in steps), cast_count)
            self.assertEqual(steps.count(dict(op='resolve')), 2)
            self.assertIn(dict(op='assert_recorded_state'), steps)


if __name__ == '__main__':
    unittest.main()
