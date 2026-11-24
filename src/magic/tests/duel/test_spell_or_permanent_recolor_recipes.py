"""Guards for exact Lace effects and independent color observations."""

import json
import unittest

from generate_cases import registry
import spell_or_permanent_recolor_recipes as model


class SpellOrPermanentRecolorTest(unittest.TestCase):
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

    def observed_colors(self, steps):
        return self.values(steps, 'assert_matching_permanents', property='color')

    def test_exact_source_clauses_and_masks(self):
        model.validate_witnesses(self.oracle, self.rows)
        expected = {59: ('B', 2), 143: ('G', 8), 196: ('W', 32), 245: ('U', 4)}
        sources = model.profiles()['sources']
        self.assertEqual({pin['registry']['csvid'] for pin in sources.values()}, set(expected))
        for name, pin in sources.items():
            record = self.oracle[name]
            color, mask = expected[pin['registry']['csvid']]
            self.assertEqual(model.parameters(record), dict(color=color, mask=mask, printed_mana='{' + color + '}',
                targets=['spell', 'permanent'], changes_mana_symbols=False))
            for bad in (record['oracle_text'].replace('spell or ', ''), record['oracle_text'] + ' Until end of turn.',
                        record['oracle_text'].replace('remain unchanged', 'change')):
                with self.assertRaises(ValueError):
                    model.parameters(dict(record, oracle_text=bad))

    def test_real_permanent_color_replaces_all_printed_colors(self):
        steps = self.case('Purelace', 'permanent_multicolored_1')['steps']
        self.assertEqual(self.observed_colors(steps), [24, 24, 32])
        self.assertIn(dict(op='choose_target', player=1, ref='target'), steps)
        steps = self.case('Deathlace', 'permanent_opposing_1')['steps']
        observed = [step for step in steps if step['op'] == 'assert_matching_permanents']
        self.assertEqual([step['player'] for step in observed], [0, 0, 0])
        self.assertEqual(self.observed_colors(steps), [16, 16, 2])
        steps = self.case('Lifelace', 'replace_previous_color_0')['steps']
        self.assertEqual(self.observed_colors(steps), [16, 16, 8, 2])
        self.assertIn(dict(op='cast', ref='helper', value=1), steps)

    def test_lace_preserves_other_characteristics_and_devotion_uses_symbols(self):
        artifact = self.case('Deathlace', 'permanent_artifact_0')['steps']
        self.assertEqual(self.observed_colors(artifact), [0, 0, 2])
        self.assertEqual(self.values(artifact, 'assert_matching_permanents', property='artifact'), [1])
        land = self.case('Purelace', 'permanent_land_mana_1')['steps']
        self.assertEqual(self.observed_colors(land), [0, 0, 32, 32])
        self.assertIn(dict(op='activate', ref='target', value=1), land)
        self.assertEqual(self.values(land, 'assert_mana', player=1, color=3)[-1], 1)
        devotion = self.case('Thoughtlace', 'printed_symbols_0')['steps']
        self.assertEqual(self.observed_colors(devotion), [16, 16, 4, 4])
        self.assertIn(dict(op='cast', ref='helper', value=1), devotion)
        self.assertEqual(self.values(devotion, 'assert_life', player=1)[-1], 17)

    def test_color_persists_across_cleanup_and_ends_when_object_leaves(self):
        steps = self.case('Purelace', 'persist_cleanup_1')['steps']
        self.assertIn(dict(op='cleanup'), steps)
        self.assertIn(dict(op='advance_turn'), steps)
        self.assertEqual(self.observed_colors(steps), [16, 16, 32, 32, 32])
        steps = self.case('Purelace', 'recast_after_change_1')['steps']
        self.assertEqual(self.values(steps, 'cast', ref='target'), [1])
        self.assertIn(dict(op='bind_hand', ref='target', player=0, value=self.rows['Furnace Whelp']['csvid']), steps)
        self.assertEqual(self.observed_colors(steps), [16, 16, 32, 16])

    def test_stack_target_observed_with_protection_and_permanent_carryover(self):
        for name, receiver in [('Deathlace', 'White Knight'), ('Lifelace', 'Spectral Lynx'),
                               ('Purelace', 'Black Knight'), ('Thoughtlace', 'Goblin Piledriver')]:
            for owner in (0, 1):
                case = self.case(name, f'spell_protection_{owner}')
                self.assertIn(dict(name=receiver, ref='receiver', player=owner, zone='battlefield'), case['setup'])
                steps = case['steps']
                self.assertIn(dict(op='choose_target', player=1 - owner, ref='receiver'), steps)
                announce = steps.index(dict(op='announce_spell', ref='spell'))
                response = steps.index(dict(op='cast_response', ref='source', value=1))
                self.assertLess(announce, response)
                self.assertIn(dict(op='resolve_response'), steps[response:])
                self.assertEqual(self.values(steps, 'assert_card', ref='receiver', property='in_play'), [1])
                control = self.case(name, f'spell_protection_control_{owner}')['steps']
                self.assertEqual(self.values(control, 'assert_card', ref='receiver', property='in_play'), [0])
                self.assertFalse(any(step['op'] == 'cast_response' for step in control))
        for mode in ('spell_becomes_permanent_1', 'spell_own_becomes_permanent_1'):
            steps = self.case('Deathlace', mode)['steps']
            self.assertEqual(self.observed_colors(steps), [2])
            self.assertIn(dict(op='cast_response', ref='source', value=1), steps)

    def test_illegal_targets_at_resolution_and_casting_negatives(self):
        for mode in ('target_hexproof', 'target_leaves', 'source_countered'):
            steps = self.case('Lifelace', mode + '_0')['steps']
            self.assertIn(dict(op='cast_response', ref='helper', value=1), steps)
            self.assertFalse(8 in self.observed_colors(steps))
        hexproof = self.case('Lifelace', 'target_hexproof_0')['steps']
        self.assertEqual(self.values(hexproof, 'assert_card', ref='target', property='power')[-1], 3)
        for mode, op, value in [('no_target', 'can_cast', 0), ('no_mana', 'cast', 0), ('same_color_eligible', 'can_cast', 1)]:
            steps = self.case('Lifelace', mode + '_1')['steps']
            self.assertIn(dict(op=op, ref='source', value=value), steps)
            self.assertFalse(any(step['op'] == 'choose_target' for step in steps))

    def test_snapshots_keep_already_paid_actions_and_real_targets(self):
        for mode, resolve in [('pending_permanent_replay', 'resolve'), ('pending_spell_replay', 'resolve_response')]:
            steps = self.case('Thoughtlace', mode + '_1')['steps']
            cut = steps.index(dict(op='capture'))
            self.assertEqual(steps[cut:].count(dict(op=resolve)), 2)
            self.assertFalse(any(step['op'] in ('cast', 'cast_response', 'announce_spell') for step in steps[cut:]))
            self.assertIn(dict(op='assert_recorded_state'), steps)


if __name__ == '__main__':
    unittest.main()
