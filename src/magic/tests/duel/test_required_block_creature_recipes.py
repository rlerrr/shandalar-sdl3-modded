import copy
import json
import unittest
from pathlib import Path

from generate_cases import registry
import required_block_creature_recipes as model


class RequiredBlockCreatureTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle = json.loads((Path(__file__).parent / 'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {row['name']: row for row in registry()}

    def case(self, mode, name='Taunting Elf'):
        record = self.oracle[name]
        declaration = dict(family=model.FAMILY, version=2, params=model.classify(record),
                           oracle_text=record['oracle_text'], variants=[mode])
        return model.expand(name, self.rows[name]['csvid'], declaration, model.spec(declaration, record),
                            [mode], self.rows, self.oracle)[0]

    @staticmethod
    def value(steps, op, **fields):
        return next(step['value'] for step in reversed(steps) if step['op'] == op
                    and all(step.get(k) == v for k, v in fields.items()))

    def count(self, steps, player, zone, name):
        return self.value(steps, 'assert_zone_multiplicity', player=player, zone=zone, index=self.rows[name]['csvid'])

    def test_exact_implemented_primaries_and_ability_witnesses(self):
        model.validate_witnesses(self.oracle, self.rows)
        self.assertEqual({self.rows[name]['csvid'] for name in model.pins()['sources']}, {2274, 4197, 10455, 15281})
        rows = copy.deepcopy(self.rows)
        rows['Flight']['handler'] = 'card_dummy'
        with self.assertRaises(ValueError):
            model.validate_witnesses(self.oracle, rows)

    def test_exact_qualified_clause_and_printed_cost_stats(self):
        record = self.oracle['Breaker of Armies']
        self.assertEqual(model.classify(record), {'mana': [8, 0, 0, 0, 0, 0], 'power': 10, 'toughness': 8})
        for text in ('A creature able to block this creature does so.', model.CLAUSE + '\nTrample',
                     'All creatures able to block this creature do so unless their controller pays {1}.'):
            with self.assertRaises(ValueError):
                model.classify(dict(record, oracle_text=text))

    def test_legal_defender_actually_refuses_and_is_required_to_block(self):
        for who in (0, 1):
            steps = self.case(f'refused_{who}')['steps']
            begin = next(i for i, s in enumerate(steps) if s['op'] == 'begin_blockers')
            advance = next(i for i, s in enumerate(steps) if s['op'] == 'advance_combat')
            self.assertEqual(steps[begin-2:begin], [dict(op='choose_optional_target', ref='blocker', player=1-who),
                                                     dict(op='choose_target', ref='blocker', player=1-who)])
            self.assertIn(dict(op='assert_unused_choices', value=0), steps[begin:advance])
            self.assertFalse(any(s['op'] == 'block' for s in steps))
            self.assertEqual(self.value(steps, 'assert_card', ref='source', property='blocked'), 1)
            self.assertEqual(self.value(steps, 'assert_card', ref='blocker', property='blocking_count'), 1)
            self.assertEqual(self.count(steps, who, 'graveyard', 'Taunting Elf'), 1)

    def test_all_eligible_block_in_either_order(self):
        for who in (0, 1):
            for mode, choice in [('two_eligible', ['blocker', 'blocker_two']), ('two_reverse', ['blocker_two', 'blocker'])]:
                steps = self.case(f'{mode}_{who}')['steps']
                before = next(i for i, s in enumerate(steps) if s['op'] == 'begin_blockers')
                self.assertEqual([s['ref'] for s in steps[:before] if s['op'] == 'choose_target'], choice)
                self.assertTrue(all(self.value(steps, 'assert_card', ref=ref, property='blocking_count') == 1 for ref in choice))
                self.assertTrue(any(s['op'] == 'advance_combat' for s in steps))

    def test_tapped_noncards_and_cannot_block_never_get_required_choice(self):
        for mode in ('all_tapped', 'noncreature', 'cannot_block', 'flying_unblocked'):
            steps = self.case(f'{mode}_0', 'Elvish Bard')['steps']
            self.assertFalse(any(s['op'] == 'choose_target' and s['player'] == 1 for s in steps))
            self.assertIn(dict(op='block', ref='blocker', other='source', value=0), steps)
            self.assertEqual(self.value(steps, 'assert_card', ref='source', property='blocked'), 0)
            self.assertEqual(self.value(steps, 'assert_life', player=1), 18)

    def test_actual_flight_cast_excludes_ground_but_requires_spider(self):
        for who in (0, 1):
            steps = self.case(f'flying_reach_{who}')['steps']
            self.assertIn(dict(op='cast', ref='flight', value=1), steps)
            self.assertIn(dict(op='choose_target', ref='source', player=who), steps)
            self.assertIn(dict(op='choose_target', ref='blocker_two', player=1-who), steps)
            self.assertEqual(self.value(steps, 'assert_card', ref='blocker', property='blocking_count'), 0)
            self.assertEqual(self.value(steps, 'assert_card', ref='blocker_two', property='blocking_count'), 1)
            self.assertEqual(self.count(steps, who, 'graveyard', 'Flight'), 1)

    def test_shroud_does_not_prevent_required_block(self):
        case = self.case('shrouded_blocker_0')
        self.assertIn('Shroud', self.oracle['Deadly Insect']['keywords'])
        self.assertTrue(any(s['name'] == 'Deadly Insect' and s['ref'] == 'blocker' for s in case['setup']))
        self.assertEqual(self.value(case['steps'], 'assert_card', ref='blocker', property='blocking_count'), 1)

    def test_other_attacker_only_can_receive_block_when_lure_idle(self):
        for who in (0, 1):
            required = self.case(f'reassign_{who}')['steps']
            alternative = self.case(f'idle_lure_block_other_{who}')['steps']
            self.assertIn(dict(op='block', ref='blocker', other='ally', value=0), required)
            self.assertIn(dict(op='block', ref='blocker', other='ally', value=1), alternative)
            self.assertEqual(self.value(required, 'assert_card', ref='source', property='blocked'), 1)
            self.assertEqual(self.value(required, 'assert_card', ref='ally', property='blocked'), 0)
            self.assertEqual(self.value(required, 'assert_life', player=1-who), 17)
            self.assertEqual(self.value(alternative, 'assert_card', ref='ally', property='blocked'), 1)
            self.assertEqual(self.value(alternative, 'assert_life', player=1-who), 20)

    def test_paid_source_becomes_eligible_after_real_turn_cycle(self):
        for who in (0, 1):
            steps = self.case(f'paid_next_turn_{who}', 'Elvish Bard')['steps']
            attack = next(i for i, s in enumerate(steps) if s['op'] == 'attack')
            self.assertIn(dict(op='cast', ref='source', value=1), steps[:attack])
            self.assertEqual(steps[attack-5:attack], [dict(op='advance_turn'), dict(op='advance_turn'),
                             dict(op='untap_step', player=who), dict(op='turn', player=who, value=0),
                             dict(op='begin_attackers')])
            self.assertEqual(self.value(steps[:attack], 'assert_mana', player=who, color=3), 0)
            self.assertIn(dict(op='attack', ref='source', value=0), self.case(f'fresh_{who}')['steps'])

    def test_after_block_replay_uses_actual_completed_declaration(self):
        steps = self.case('after_block_replay_0')['steps']
        a, b, c = [next(i for i, s in enumerate(steps) if s['op'] == op) for op in ('capture', 'record_state', 'restore')]
        self.assertEqual(self.value(steps[:a], 'assert_card', ref='source', property='blocked'), 1)
        self.assertEqual(steps[a+1:b], steps[c+1:-1])
        self.assertEqual(steps[a+1]['op'], 'combat_damage')


if __name__ == '__main__':
    unittest.main()
