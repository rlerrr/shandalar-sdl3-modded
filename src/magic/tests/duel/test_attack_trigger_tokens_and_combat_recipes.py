"""Focused guards for exact attack-token and combat-rider expansion."""

from copy import deepcopy
import unittest

import attack_trigger_tokens_and_combat_recipes as model


class AttackTokenCombatTests(unittest.TestCase):
    def setUp(self):
        pins = model.profiles()
        cards = pins['sources'] | pins['witnesses']
        self.oracle = {name: item['oracle'] for name, item in cards.items()}
        self.rows = {name: item['registry'] for name, item in cards.items()}
        for name, item in pins['token_identities'].items():
            self.rows[name] = item['registry']
            if item['available_oracle_token_record'] is not None:
                self.oracle[name] = item['available_oracle_token_record']

    def selection(self, name, variant):
        row = self.rows[name]
        record = self.oracle[name]
        shape = model.classify(record, row['csvid'])
        declaration = dict(family=model.FAMILY, version=2, params=shape,
                           oracle_text=record['oracle_text'], variants=[variant])
        return row, record, shape, declaration

    def test_whole_record_guard(self):
        row, record, shape, declaration = self.selection('Preeminent Captain', 'soldier_0')
        self.assertEqual(shape['csvid'], row['csvid'])
        self.assertEqual(model.spec(declaration, record), dict(shape, family=model.FAMILY))
        with self.assertRaises(ValueError):
            model.spec(declaration, dict(record, oracle_text=record['oracle_text'].splitlines()[1]))
        with self.assertRaises(ValueError):
            model.spec(dict(declaration, version=2.0), record)
        with self.assertRaises(ValueError):
            model.spec(dict(declaration, variants=['decline_1']), record)

    def test_independent_identity_and_witness_pins(self):
        model.validate_witnesses(self.oracle, self.rows)
        bad = deepcopy(self.rows)
        bad['Twin']['csvid'] = 18822
        with self.assertRaises(ValueError):
            model.validate_witnesses(self.oracle, bad)
        bad = deepcopy(self.oracle)
        bad['Giant Growth']['oracle_text'] = 'Target creature gets +4/+4 until end of turn.'
        with self.assertRaises(ValueError):
            model.validate_witnesses(bad, self.rows)

    def test_real_combat_and_token_observers(self):
        row, _, shape, declaration = self.selection('Gemini Engine', 'boosted_0')
        case, = model.expand('Gemini Engine', row['csvid'], declaration, shape, declaration['variants'], self.rows, self.oracle)
        steps = case['steps']
        ops = [step['op'] for step in steps]
        self.assertLess(ops.index('finish_attackers'), ops.index('combat_damage'))
        self.assertLess(ops.index('combat_damage'), ops.index('end_combat'))
        self.assertIn(dict(op='assert_matching_permanents', player=0, index=18846, property='tapped', value=0), steps)
        self.assertTrue(all(item['name'] != 'Twin' for item in case['setup']))

    def test_supported_scope_is_explicit(self):
        self.assertEqual(len(model.PRINTED), 15)
        row, _, shape, declaration = self.selection('Lightning Runner', 'pay_1')
        case, = model.expand('Lightning Runner', row['csvid'], declaration, shape, declaration['variants'], self.rows, self.oracle)
        self.assertIn(dict(op='assert_energy', player=1, value=0), case['steps'])
        self.assertEqual(sum(step['op'] == 'cast' and step['ref'].startswith('servant_') for step in case['steps']), 3)
        self.assertTrue(case['_recipe']['untested_components'])

    def test_extra_combat_uses_real_schedule_then_declares_without_phase_reset(self):
        row, _, shape, declaration = self.selection('Hellkite Charger', 'extra_pay_0')
        case, = model.expand('Hellkite Charger', row['csvid'], declaration, shape,
                             declaration['variants'], self.rows, self.oracle)
        steps = case['steps']
        first = steps.index(dict(op='end_combat_step', value=1))
        second = steps.index(dict(op='end_combat_step', value=0))
        repeated = steps[first + 1:second]
        self.assertEqual([item['ref'] for item in repeated if item['op'] == 'attack'], ['source', 'partner'])
        self.assertFalse({'turn', 'advance_turn', 'begin_attackers', 'begin_required_attackers'} &
                         {item['op'] for item in repeated})
        self.assertIn(dict(op='assert_card', ref='source', property='tapped', value=1), repeated)

    def test_invalid_captain_pick_and_bounded_scourge_transition(self):
        for name, variant in (('Preeminent Captain', 'non_soldier_1'), ('Scourge of the Throne', 'extra_foe_ahead_0')):
            row, _, shape, declaration = self.selection(name, variant)
            case, = model.expand(name, row['csvid'], declaration, shape, declaration['variants'], self.rows, self.oracle)
            steps = case['steps']
            if name == 'Preeminent Captain':
                non_soldier = self.rows['Prodigal Sorcerer']['csvid']
                self.assertIn(dict(op='choose_optional_zone', player=1, value=non_soldier), steps)
                self.assertIn(dict(op='assert_offered_legal_card', index=non_soldier, value=0), steps)
            else:
                transition = steps.index(dict(op='end_combat_step', value=1))
                self.assertFalse({'attack', 'begin_attackers', 'finish_attackers'} &
                                 {item['op'] for item in steps[transition + 1:]})
                self.assertIn(dict(op='assert_card', ref='source', property='plus1_counters', value=1), steps[:transition])
