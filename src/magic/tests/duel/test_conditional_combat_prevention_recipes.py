import json
import unittest
from pathlib import Path

from conditional_combat_prevention_recipes import FAMILY, classify, expand, spec, validate_witnesses, variants
from generate_cases import registry


class ConditionalCombatPreventionRecipesTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        oracle = Path(__file__).parent / 'data/oracle.json'
        cls.oracle = json.loads(oracle.read_text(encoding='utf8'))['cards']
        cls.rows = {row['name']: row for row in registry()}

    def declaration(self, name):
        record = self.oracle[name]
        d = dict(family=FAMILY, version=2, oracle_text=record['oracle_text'], params=classify(record), variants=[])
        return d, spec(d, record)

    def steps(self, name, variant):
        decl, p = self.declaration(name)
        return expand(name, self.rows[name]['csvid'], decl, p, [variant], self.rows, self.oracle)[0]['steps']

    def values(self, steps, op, **fields):
        return [step['value'] for step in steps if step['op'] == op and all(step.get(k) == v for k, v in fields.items())]

    def test_whole_oracle_and_independent_source_pins(self):
        expected = [('Enchanted Being', 'enchanted', True), ('Wall of Putrid Flesh', 'enchanted', False),
                    ('Wall of Shadows', 'blocking', False), ('Marble Priest', 'wall', True)]
        for name, condition, combat_only in expected:
            record = self.oracle[name]
            decl, params = self.declaration(name)
            self.assertEqual((params['condition'], params['combat_only']), (condition, combat_only))
            with self.assertRaises(ValueError):
                classify(dict(record, oracle_text=record['oracle_text'] + '\nDraw a card.'))
            with self.assertRaises(ValueError):
                spec(dict(decl, version=2.0), record)
        validate_witnesses(self.oracle, self.rows)
        with self.assertRaises(ValueError):
            validate_witnesses(dict(self.oracle, **{'Wall of Heat': dict(self.oracle['Wall of Heat'], power='3')}), self.rows)

    def test_real_aura_and_removal_change_combat_prevention(self):
        protected = self.steps('Enchanted Being', 'combat_match_0')
        self.assertEqual(self.values(protected, 'assert_card', ref='source', property='damage')[-1], 0)
        self.assertEqual(self.values(protected, 'assert_card', ref='other', property='damage')[-1], 2)
        removed = self.steps('Enchanted Being', 'aura_removed_0')
        self.assertEqual([s['ref'] for s in removed if s['op'] == 'cast'], ['aura', 'disenchant'])
        self.assertEqual(self.values(removed, 'assert_zone_multiplicity', player=0, zone='graveyard', index=624)[-1], 1)

    def test_enchanted_creature_ping_distinguishes_combat_only(self):
        for name, damage in (('Enchanted Being', 1), ('Wall of Putrid Flesh', 0)):
            steps = self.steps(name, 'enchanted_ping_1')
            self.assertEqual(self.values(steps, 'activate', ref='other'), [1])
            self.assertEqual(self.values(steps, 'assert_card', ref='source', property='damage')[-1], damage)
        control = self.steps('Wall of Putrid Flesh', 'unenchanted_ping_1')
        self.assertEqual(self.values(control, 'assert_card', ref='source', property='damage')[-1], 1)

    def test_wall_of_shadows_real_block_controls_ping_and_wall_only_targets(self):
        blocked = self.steps('Wall of Shadows', 'blocked_ping_0')
        ops = [s['op'] for s in blocked]
        self.assertLess(ops.index('block'), ops.index('activate'))
        self.assertLess(ops.index('activate'), ops.index('combat_damage'))
        self.assertEqual(self.values(blocked, 'assert_card', ref='source', property='damage')[-1], 0)
        unblocked = self.steps('Wall of Shadows', 'unblocked_ping_0')
        self.assertEqual(self.values(unblocked, 'assert_zone_multiplicity', player=0, zone='graveyard', index=847)[-1], 1)
        abilities = self.steps('Wall of Shadows', 'wall_ability_denied_1')
        self.assertEqual(self.values(abilities, 'probe_activation_target', ref='demolition', other='source'), [0])
        self.assertEqual(self.values(abilities, 'probe_activation_target', ref='demolition', other='eligible_wall'), [1])
        tunnel = self.steps('Wall of Shadows', 'wall_spell_denied_0')
        self.assertEqual(self.values(tunnel, 'can_cast', ref='tunnel'), [0])

    def test_putrid_flesh_white_protection_and_defender(self):
        white = self.steps('Wall of Putrid Flesh', 'white_combat_1')
        self.assertEqual(self.values(white, 'assert_card', ref='source', property='damage')[-1], 0)
        self.assertEqual(self.values(self.steps('Wall of Putrid Flesh', 'white_aura_denied_1'), 'can_cast', ref='white_aura'), [0])
        self.assertEqual(self.values(self.steps('Wall of Putrid Flesh', 'black_aura_control_1'),
                                     'assert_card', ref='source', property='toughness')[-1], 5)
        self.assertEqual(self.values(self.steps('Wall of Putrid Flesh', 'source_attack_1'), 'attack', ref='source'), [0])

    def test_marble_priest_uses_mandatory_blocker_api_and_receipts(self):
        multiple = self.steps('Marble Priest', 'multiple_walls_0')
        start = next(i for i, step in enumerate(multiple) if step['op'] == 'begin_blockers')
        self.assertEqual([s['ref'] for s in multiple[:start] if s['op'] == 'choose_target'], ['other', 'extra_wall'])
        self.assertFalse(any(s['op'] == 'block' for s in multiple))
        for ref in ('other', 'extra_wall'):
            self.assertEqual(self.values(multiple[start:], 'assert_card', ref=ref, property='blocking_count'), [1])
        control = self.steps('Marble Priest', 'tapped_wall_0')
        self.assertFalse(any(s['op'] == 'choose_target' for s in control))
        self.assertEqual(self.values(control, 'assert_life', player=1)[-1], 17)

    def test_selected_matrix_and_replay(self):
        counts = {'Enchanted Being': 24, 'Wall of Putrid Flesh': 30, 'Wall of Shadows': 30, 'Marble Priest': 20}
        for name, count in counts.items():
            decl, params = self.declaration(name)
            cases = expand(name, self.rows[name]['csvid'], decl, params, variants(params), self.rows, self.oracle)
            self.assertEqual(len({c['name'] for c in cases}), count)
        steps = self.steps('Marble Priest', 'blocked_snapshot_0')
        start = next(i for i, s in enumerate(steps) if s['op'] == 'capture')
        record = next(i for i, s in enumerate(steps) if s['op'] == 'record_state')
        restore = next(i for i, s in enumerate(steps) if s['op'] == 'restore')
        self.assertEqual(steps[start+1:record], steps[restore+1:-1])
