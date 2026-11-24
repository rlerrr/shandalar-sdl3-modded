import json
from pathlib import Path
import unittest

import zero_cost_creature_recipes as model
from generate_cases import registry


class ZeroCostCreatureRecipeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        oracle = Path(__file__).with_name('data') / 'oracle.json'
        cls.oracle = json.loads(oracle.read_text(encoding='utf8'))['cards']
        cls.rows = {card['name']: card for card in registry()}
        model.validate_witnesses(cls.oracle, cls.rows)

    def steps(self, name, variant):
        record = self.oracle[name]
        declaration = dict(family=model.FAMILY, version=2, oracle_text=record['oracle_text'],
                           params=model.parameters(record), variants=[variant])
        case, = model.expand(name, self.rows[name]['csvid'], declaration, model.spec(declaration, record),
                             [variant], self.rows, self.oracle)
        return case['steps']

    def test_reject_changed_rules_and_cost(self):
        sphere = self.oracle['Shield Sphere']
        for change in (dict(oracle_text='Defender'), dict(mana_cost='{1}'), dict(colors=['R'])):
            with self.assertRaises(ValueError):
                model.parameters(sphere | change)

    def test_cast_retains_all_six_nonempty_pools_and_printed_color(self):
        for owner in (0, 1):
            steps = self.steps('Crookshank Kobolds', f'surplus_{owner}')
            self.assertEqual([step['value'] for step in steps if step['op'] == 'assert_mana'
                              and step['player'] == owner], [2, 3, 4, 5, 6, 7])
            self.assertIn(dict(op='assert_matching_permanents', player=owner, index=605, property='color', value=16), steps)
            self.assertIn(dict(op='attack', ref='source', value=0), steps)

    def test_lord_witness_checks_subtype_and_excludes_bears(self):
        steps = self.steps('Crimson Kobolds', 'kobold_lord_1')
        power = {step['ref']: step['value'] for step in steps if step.get('property') == 'power'}
        self.assertEqual(power, {'source': 1, 'lord': 1, 'other': 2})

    def test_shield_sphere_loses_toughness_only_after_real_blocks(self):
        steps = self.steps('Shield Sphere', 'repeat_block_0')
        self.assertEqual(sum(step['op'] == 'block' for step in steps), 2)
        self.assertEqual([step['value'] for step in steps if step.get('property') == 'toughness'], [6, 5, 5, 5, 4, 4])
        control = self.steps('Shield Sphere', 'unblocked_0')
        self.assertEqual([step['value'] for step in control if step.get('property') == 'toughness'], [6, 6, 6])
        self.assertIn(dict(op='attack', ref='source', value=0), self.steps('Shield Sphere', 'attack_0'))

    def test_replay_snapshots_real_pending_cast(self):
        steps = self.steps('Kobolds of Kher Keep', 'replay_1')
        ops = [step['op'] for step in steps]
        self.assertLess(ops.index('cast'), ops.index('capture'))
        self.assertLess(ops.index('capture'), ops.index('resolve'))
        self.assertEqual(ops.count('resolve'), 2)
        self.assertIn('assert_recorded_state', ops)
