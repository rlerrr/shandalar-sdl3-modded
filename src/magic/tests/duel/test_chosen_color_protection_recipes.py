import json
import unittest
from pathlib import Path

from chosen_color_protection_recipes import COLORS, FAMILY, classify, expand, spec, validate_witnesses, variants
from generate_cases import registry


class ChosenColorProtectionRecipesTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle = json.loads((Path(__file__).parent / 'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {row['name']: row for row in registry()}

    def declaration(self, name):
        record = self.oracle[name]
        d = dict(family=FAMILY, version=2, oracle_text=record['oracle_text'], params=classify(record), variants=[])
        return d, spec(d, record)

    def steps(self, name, variant):
        declaration, params = self.declaration(name)
        return expand(name, self.rows[name]['csvid'], declaration, params, [variant], self.rows, self.oracle)[0]['steps']

    def values(self, steps, op, **fields):
        return [step['value'] for step in steps if step['op'] == op and all(step.get(k) == v for k, v in fields.items())]

    def test_whole_clauses_and_independently_pinned_sources(self):
        for name in ('Stave Off', 'Mother of Runes', 'Benevolent Bodyguard', 'Midvast Protector', 'Brave the Elements', 'Gods Willing'):
            record = self.oracle[name]
            declaration, params = self.declaration(name)
            self.assertEqual(params['scry'], int(name == 'Gods Willing'))
            with self.assertRaises(ValueError):
                classify(dict(record, oracle_text=record['oracle_text'] + '\nDraw a card.'))
            with self.assertRaises(ValueError):
                spec(dict(declaration, version=2.0), record)
        for name in ('Blessed Breath', 'Bathe in Light', 'Center Soul', 'Emerge Unscathed'):
            with self.assertRaises(ValueError):
                classify(self.oracle[name])
        validate_witnesses(self.oracle, self.rows)
        with self.assertRaises(ValueError):
            validate_witnesses(dict(self.oracle, **{'Hill Giant': dict(self.oracle['Hill Giant'], colors=['W'])}), self.rows)

    def test_actual_matching_blocked_color_and_nonmatching_block(self):
        for color, menu_index in COLORS.items():
            steps = self.steps('Stave Off', f'block_{color}_1')
            self.assertEqual(self.values(steps, 'choose_color', player=1), [menu_index])
            self.assertEqual(self.values(steps, 'block', ref='combatant'), [0])
            self.assertEqual(self.values(steps, 'assert_life', player=0)[-1], 18)
        legal = self.steps('Stave Off', 'block_other_1')
        self.assertEqual(self.values(legal, 'block', ref='combatant'), [1])
        self.assertEqual(self.values(legal, 'assert_card', ref='selected', property='damage')[-1], 3)
        defending = self.steps('Mother of Runes', 'defend_red_0')
        self.assertEqual(self.values(defending, 'block', ref='selected'), [1])
        self.assertEqual(self.values(defending, 'assert_card', ref='selected', property='damage')[-1], 0)
        self.assertEqual(self.values(defending, 'assert_card', ref='combatant', property='damage')[-1], 2)

    def test_probe_rejects_only_selected_source_color(self):
        for color in ('red', 'blue'):
            steps = self.steps('Mother of Runes', f'probe_{color}_0')
            for witness in ('red', 'blue'):
                self.assertEqual(self.values(steps, 'probe_activation_target', ref=f'{witness}_pinger', other='selected'),
                                 [int(color != witness)])
        own_only = self.steps('Mother of Runes', 'foreign_target_probe_1')
        self.assertEqual(self.values(own_only, 'probe_activation_target', ref='source', other='foreign'), [0])

    def test_red_sweep_uses_control_not_name_and_filters_white_team(self):
        steps = self.steps('Brave the Elements', 'effect_0')
        actual = {ref: self.values(steps, 'assert_card', ref=ref, property='damage')[-1]
                  for ref in ('selected', 'other_white', 'nonwhite', 'foreign_white')}
        self.assertEqual(actual, dict(selected=0, other_white=0, nonwhite=2, foreign_white=2))
        foreign = self.steps('Stave Off', 'foreign_target_1')
        self.assertTrue(any(s == dict(op='choose_target', player=1, ref='selected') for s in foreign))
        self.assertEqual(self.values(foreign, 'assert_card', ref='selected', property='damage')[-1], 0)

    def test_aura_and_pending_spell_exercise_different_protection_rules(self):
        steps = self.steps('Stave Off', 'aura_drop_0')
        self.assertEqual(self.values(steps, 'assert_zone_multiplicity', player=0, zone='graveyard', index=968)[-1], 1)
        self.assertEqual(self.values(steps, 'choose_color', player=0), [5])
        response = self.steps('Benevolent Bodyguard', 'bolt_response_0')
        self.assertIn(2, self.values(response, 'assert_stack'))
        self.assertEqual(self.values(response, 'assert_zone_multiplicity', player=0, zone='graveyard', index=5912)[-1], 1)
        self.assertEqual(self.values(response, 'assert_zone_multiplicity', player=1, zone='graveyard', index=145)[-1], 1)
        self.assertEqual(self.values(response, 'assert_card', ref='selected', property='damage')[-1], 0)

    def test_paid_entry_cast_then_target_then_color_and_scry_only_on_legal_resolution(self):
        entry = self.steps('Midvast Protector', 'self_target_1')
        input_ops = [s['op'] for s in entry if s['op'] in ('cast', 'choose_target', 'choose_color')]
        self.assertEqual(input_ops, ['cast', 'choose_target', 'choose_color', 'cast'])
        self.assertEqual(self.values(entry, 'assert_card', ref='source', property='damage')[-1], 0)
        scry = self.steps('Gods Willing', 'scry_bottom_0')
        self.assertEqual(self.values(scry, 'assert_scry_result', player=0), [1])
        self.assertEqual(self.values(scry, 'assert_zone_card', player=0, zone='library', index=2)[-1], 126)
        lost = self.steps('Gods Willing', 'target_lost_pending_0')
        self.assertFalse(any(step['op'] in ('choose_color', 'assert_scry_result') for step in lost))

    def test_real_turn_expiry_and_both_controller_matrix(self):
        expiry = self.steps('Mother of Runes', 'after_turn_0')
        self.assertEqual([step['player'] for step in expiry if step['op'] == 'untap_step'], [1, 0])
        self.assertEqual(self.values(expiry, 'assert_card', ref='selected', property='damage'), [0, 0, 2])
        for name, count in [('Stave Off', 38), ('Mother of Runes', 48), ('Benevolent Bodyguard', 44),
                            ('Midvast Protector', 32), ('Brave the Elements', 36), ('Gods Willing', 39)]:
            declaration, params = self.declaration(name)
            cases = expand(name, self.rows[name]['csvid'], declaration, params, variants(params), self.rows, self.oracle)
            self.assertEqual(len({case['name'] for case in cases}), count)
