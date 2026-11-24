"""Focused guards for the targeted Delirium cases."""
import json
import unittest

from generate_cases import registry
import delirium_targeted_spells_and_aura_recipes as model


class DeliriumTargetedTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        data = model.PROFILE.parent
        cls.oracle = json.loads((data / 'oracle.json').read_bytes())['cards']
        cls.rows = {row['name']: row for row in registry()}
        cls.homes = {}
        for name, pin in model.profiles()['sources'].items():
            home, = (data / 'cases/cards').glob(f"*/{pin['registry']['csvid']:05d}-*.json")
            cls.homes[name] = json.loads(home.read_bytes())['recipes'][0]

    def duel(self, name, variant):
        declaration = self.homes[name]
        shape = model.spec(declaration, self.oracle[name])
        return model.build(name, self.rows[name]['csvid'], declaration, shape, variant, self.rows, self.oracle)

    def test_full_pins_and_equal_size_threshold(self):
        model.validate_witnesses(self.oracle, self.rows)
        for name, decl in self.homes.items():
            self.assertEqual(decl['variants'], model.variants(model.spec(decl, self.oracle[name])))
            with self.assertRaises(ValueError):
                model.spec(decl, dict(self.oracle[name], oracle_text='Delirium'))
            graves = []
            for mode in ('three_duplicates', 'four_types', 'opponent_only'):
                setup = self.duel(name, mode + '_1')['setup']
                graves.append([item['name'] for item in setup if item['player'] == 1 and item['zone'] == 'graveyard'])
            self.assertEqual(list(map(len, graves)), [4, 4, 4])
            self.assertEqual([len(model.types_for(grave, self.oracle)) for grave in graves], [3, 4, 3])

    def test_actual_short_payment_and_paid_counter(self):
        for name in self.homes:
            short = self.duel(name, 'short_funding_0')['steps']
            self.assertIn(dict(op='cast', ref='source', value=0), short)
            self.assertFalse(any(step['op'].startswith('choose_') or step['op'] == 'resolve' for step in short))
            sequence = self.duel(name, 'countered_1')['steps']
            self.assertEqual([step['op'] for step in sequence if step['op'] in ('announce_spell', 'cast_response', 'resolve_response')],
                             ['announce_spell', 'cast_response', 'resolve_response'])

    def test_timing_and_required_or_optional_choice(self):
        for name in ('Pick the Brain', 'Whispers of Emrakul'):
            steps = self.duel(name, 'lose_fourth_0')['steps']
            response = next(index for index, step in enumerate(steps) if step['op'] == 'cast_response')
            resolve = next(index for index, step in enumerate(steps) if step['op'] == 'resolve_response')
            self.assertEqual(steps[response]['ref'], 'reclaim')
            self.assertEqual(steps[response-1]['value'], self.rows[model.AMULET]['csvid'])
            self.assertLess(response, resolve)
            picks = [step for step in steps[response+1:resolve] if step['op'] == 'choose_zone_card']
            self.assertEqual([step['value'] for step in picks], [self.rows[model.NONLAND]['csvid']] if name == 'Pick the Brain' else [])
        partial = self.duel('Pick the Brain', 'leave_partial_1')['steps']
        self.assertEqual([step['op'] for step in partial if step['op'] in ('choose_zone_card', 'choose_zone')],
                         ['choose_zone_card', 'choose_zone_card', 'choose_zone', 'choose_zone_card'])
        aura = self.duel('Strange Augmentation', 'lose_fourth_0')['steps']
        self.assertEqual([step['value'] for step in aura if step.get('ref') == 'target' and step.get('property') == 'power'], [5, 3])

    def test_replay_preserves_one_paid_cast_and_exact_settlement(self):
        for name in self.homes:
            steps = self.duel(name, 'replay_1')['steps']
            self.assertEqual(sum(step['op'] == 'cast' and step.get('ref') == 'source' for step in steps), 1)
            ops = [step['op'] for step in steps]
            capture, record, restore, ai, compare = map(ops.index, ('capture', 'record_state', 'restore', 'ai_roundtrip', 'assert_recorded_state'))
            self.assertLess(record, restore)
            self.assertLess(restore, ai)
            self.assertEqual(steps[capture+1:record], steps[ai+1:compare])


if __name__ == '__main__':
    unittest.main()
