"""Guard whole Oracle records and distinct sabotage combat boundaries."""

from copy import deepcopy
import unittest

import attack_follow_on_sabotage_and_poison_recipes as model


class SabotagePoisonTests(unittest.TestCase):
    def setUp(self):
        pins = model.profiles()
        cards = pins['sources'] | pins['witnesses']
        self.oracle = {name: item['oracle'] for name, item in cards.items()}
        self.rows = {name: item['registry'] for name, item in cards.items()}

    def selection(self, name, variant):
        row = self.rows[name]
        record = self.oracle[name]
        shape = model.classify(record, row['csvid'])
        declaration = dict(family=model.FAMILY, version=2, params=shape,
                           oracle_text=record['oracle_text'], variants=[variant])
        return row, record, shape, declaration

    def steps(self, name, variant):
        row, _, shape, declaration = self.selection(name, variant)
        case, = model.expand(name, row['csvid'], declaration, shape, declaration['variants'], self.rows, self.oracle)
        return case['steps']

    def test_exact_source_and_witness_records(self):
        _, record, shape, declaration = self.selection("Suq'Ata Assassin", 'black_blocker_1')
        self.assertEqual(model.spec(declaration, record), dict(shape, family=model.FAMILY))
        with self.assertRaises(ValueError):
            model.spec(declaration, dict(record, oracle_text=record['oracle_text'].splitlines()[1]))
        with self.assertRaises(ValueError):
            model.spec(dict(declaration, version=2.0), record)
        model.validate_witnesses(self.oracle, self.rows)
        bad = deepcopy(self.oracle)
        bad[model.SHADOW]['oracle_text'] = 'Flying'
        with self.assertRaises(ValueError):
            model.validate_witnesses(bad, self.rows)

    def test_positive_poison_and_loss_precede_combat_damage(self):
        for card, variant, op, value in (('Crypt Cobra', 'unblocked_0', 'assert_poison', 1),
                                          ('Guiltfeeder', 'three_0', 'assert_life', 17)):
            steps = self.steps(card, variant)
            begin = steps.index(dict(op='advance_combat'))
            damage = steps.index(dict(op='combat_damage'))
            self.assertIn(dict(op=op, player=1, value=value), steps[begin:damage])

    def test_end_of_combat_pump_expiry_stays_separate_from_turn_end(self):
        steps = self.steps('Murk Dwellers', 'unblocked_0')
        end = steps.index(dict(op='end_combat'))
        self.assertIn(dict(op='assert_card', ref='source', property='power', value=4), steps[:end])
        self.assertIn(dict(op='assert_card', ref='source', property='power', value=2), steps[end:])
        self.assertNotIn('advance_turn', [step['op'] for step in steps[end:]])

    def test_actual_targets_sacrifice_and_owner_return(self):
        steps = self.steps('Goblin Grenadiers', 'own_targets_1')
        activation = steps.index(dict(op='activate', ref='source', value=1))
        self.assertEqual([item['ref'] for item in steps[activation - 2:activation]], ['own_creature', 'own_land'])
        self.assertIn(dict(op='assert_card', ref='source', property='in_play', value=0), steps[activation:])
        steps = self.steps('Thalakos Deceiver', 'owner_return_0')
        bind = next(i for i, step in enumerate(steps) if step['op'] == 'bind_permanent_copy')
        self.assertEqual(steps[bind]['player'], 0)
        self.assertIn(dict(op='choose_target', ref='controlled', player=0), steps[bind:])
        self.assertIn(dict(op='assert_zone_multiplicity', player=1, zone='hand', index=self.rows[model.CREATURE]['csvid'], value=1), steps[bind:])
