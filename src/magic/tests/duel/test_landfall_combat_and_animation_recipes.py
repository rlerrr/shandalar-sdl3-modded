"""Oracle and generated-step guards for the combat Landfall model."""
import copy
import unittest

import landfall_combat_and_animation_recipes as model


class LandfallCombatTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        profile = model.profile()
        cls.sources = profile['sources']
        cls.oracle = {name: pin['oracle'] for name, pin in (profile['sources'] | profile['witnesses']).items()}
        cls.rows = {name: pin['registry'] for name, pin in (profile['sources'] | profile['witnesses']).items()}

    def case(self, name, variant):
        record = self.oracle[name]
        declaration = dict(family=model.FAMILY, version=2, oracle_text=record['oracle_text'],
                           params=model.parameters(record), variants=[variant])
        shape = model.spec(declaration, record)
        case, = model.expand(name, self.rows[name]['csvid'], declaration, shape, [variant], self.rows, self.oracle)
        return case

    def test_whole_oracle_and_independent_pins(self):
        model.validate_witnesses(self.oracle, self.rows)
        for name, record in self.oracle.items():
            if name not in self.sources:
                continue
            stale = copy.deepcopy(record)
            stale['oracle_text'] += '\nFlying'
            with self.subTest(name=name), self.assertRaises(ValueError):
                model.parameters(stale)

    def test_actual_paid_source_and_both_seats(self):
        for name in self.sources:
            for owner in (0, 1):
                with self.subTest(name=name, owner=owner):
                    case = self.case(name, f'opponent_land_{owner}')
                    source, = [entry for entry in case['setup'] if entry['ref'] == 'source']
                    land, = [entry for entry in case['setup'] if entry['ref'] == 'land']
                    self.assertEqual((source['zone'], source['player'], land['zone'], land['player']),
                                     ('hand', owner, 'hand', 1 - owner))
                    ops = [step['op'] for step in case['steps']]
                    self.assertLess(ops.index('cast'), ops.index('play_land'))
                    self.assertIn('advance_turn', ops[ops.index('cast'):ops.index('play_land')])

    def test_menace_and_basilisk_observe_real_declarations(self):
        for name, variant, expected in (
                ('Geyserfield Stalker', 'menace_one_0', 0), ('Geyserfield Stalker', 'menace_two_1', 1),
                ('Turntimber Basilisk', 'tapped_target_1', 0), ('Turntimber Basilisk', 'forced_block_0', 1)):
            with self.subTest(name=name, variant=variant):
                steps = self.case(name, variant)['steps']
                boundary = next(i for i, step in enumerate(steps) if step['op'] == 'advance_combat')
                self.assertEqual(steps[boundary + 1]['property'], 'blocked')
                self.assertEqual(steps[boundary + 1]['value'], expected)
                if name == 'Turntimber Basilisk':
                    self.assertNotIn('block', [step['op'] for step in steps[:-1]])

    def test_animation_checks_subtype_land_mana_and_haste(self):
        for name in ('Embodiment of Fury', 'Embodiment of Insight'):
            for variant, action in [('land_mana_0', 'activate'), ('animate_combat_1', 'attack')]:
                steps = self.case(name, variant)['steps']
                observed = next(i for i, step in enumerate(steps)
                                if step.get('property') == 'subtype_Elemental' and step['value'] == 1)
                actual = next(i for i, step in enumerate(steps) if step['op'] == action and step['ref'] == 'land')
                self.assertLess(observed, actual)
                if action == 'attack':
                    self.assertEqual(steps[actual + 1]['value'], int(name == 'Embodiment of Fury'))


if __name__ == '__main__':
    unittest.main()
