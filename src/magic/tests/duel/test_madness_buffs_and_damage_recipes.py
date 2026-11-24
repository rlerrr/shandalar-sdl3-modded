"""Guard whole-card Madness inputs and the distinct additional cases."""
import json
import unittest

from generate_cases import registry
import madness_buffs_and_damage_recipes as model


class MadnessBuffsAndDamageTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        data = model.PROFILE.parent
        cls.oracle = json.loads((data / 'oracle.json').read_bytes())['cards']
        cls.rows = {row['name']: row for row in registry()}
        cls.homes = {}
        for name, pin in model.profiles()['sources'].items():
            home, = (data / 'cases/cards').glob(f"*/{pin['registry']['csvid']:05d}-*.json")
            cls.homes[name] = json.loads(home.read_bytes())['recipes'][0]

    def steps(self, name, variant):
        declaration = self.homes[name]
        shape = model.spec(declaration, self.oracle[name])
        return model.build(name, self.rows[name]['csvid'], declaration, shape, variant, self.rows, self.oracle)['steps']

    def test_complete_pins_and_distinct_variants(self):
        model.validate_witnesses(self.oracle, self.rows)
        for name, declaration in self.homes.items():
            record = self.oracle[name]
            self.assertEqual(declaration['variants'], model.variants(model.spec(declaration, record)))
            with self.assertRaises(ValueError):
                model.spec(declaration, dict(record, mana_cost='{0}'))

    def test_countered_cast_preserves_cast_choice_without_effect(self):
        for name in self.homes:
            steps = self.steps(name, 'normal_countered_1')
            self.assertEqual([s['op'] for s in steps if s['op'] in ('announce_spell', 'cast_response', 'resolve_response')],
                             ['announce_spell', 'cast_response', 'resolve_response'])
            first = steps.index(dict(op='cast', ref='source', value=1))
            self.assertFalse(any(s['op'] in ('choose_target', 'combat_damage') for s in steps[first+1:]))

    def test_real_defense_and_offturn_empty_hand(self):
        for name in self.homes:
            steps = self.steps(name, 'offturn_paid_0')
            activate = steps.index(dict(op='activate', ref='looter', value=1))
            self.assertEqual([s['player'] for s in steps[:activate] if s['op'] == 'turn'][-1], 1)
        for name in ('Strength of Isolation', 'Strength of Lunacy'):
            steps = self.steps(name, 'offturn_paid_0')
            self.assertIn(dict(op='block', ref='host', other='protected_blocker', value=1), steps)
            self.assertIn(dict(op='assert_card', ref='host', property='damage', value=0), steps)
        abandon = self.steps('Abandon Reason', 'offturn_paid_1')
        self.assertIn(dict(op='block', ref='first', other='first_blocker', value=1), abandon)
        priest = self.steps('Bloodhall Priest', 'offturn_paid_0')
        self.assertLess(priest.index(dict(op='play_land', ref='drawn_land', value=1)),
                        priest.index(dict(op='choose_player', player=0, value=1)))

    def test_replay_executes_discard_and_effect_twice(self):
        for name in self.homes:
            steps = self.steps(name, 'replay_1')
            ops = [s['op'] for s in steps]
            capture, record, ai, restore, compare = map(ops.index, ('capture', 'record_state', 'ai_roundtrip', 'restore', 'assert_recorded_state'))
            self.assertLess(record, ai)
            self.assertLess(ai, restore)
            self.assertIn(dict(op='activate', ref='looter', value=1), steps[capture+1:record])
            self.assertEqual(steps[capture+1:record], steps[restore+1:compare])


if __name__ == '__main__':
    unittest.main()
