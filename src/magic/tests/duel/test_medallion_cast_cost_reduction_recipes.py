"""Focused guards for exact Medallion wording and paid-spell boundaries."""

import json
import unittest

from generate_cases import registry
import medallion_cast_cost_reduction_recipes as model


class MedallionCastCostReductionTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle = json.loads((model.PROFILE_PATH.parent / 'oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {row['name']: row for row in registry()}

    def case(self, kind, mode):
        name = f'{kind} Medallion'
        record = self.oracle[name]
        declaration = dict(family=model.FAMILY, version=2, oracle_text=record['oracle_text'],
                           params=model.parameters(record), variants=[mode])
        return model.expand(name, self.rows[name]['csvid'], declaration, model.spec(declaration, record),
                            [mode], self.rows, self.oracle)[0]

    @staticmethod
    def values(steps, op, **fields):
        return [step['value'] for step in steps if step['op'] == op and all(step.get(key) == value for key, value in fields.items())]

    def test_source_colors_and_complete_clause_are_pinned(self):
        model.validate_witnesses(self.oracle, self.rows)
        source_colors = {3132: 'G', 3180: 'B', 3225: 'W', 3256: 'R', 3261: 'U'}
        sources = model.profiles()['sources']
        self.assertEqual(len(sources), len(source_colors))
        for name, pin in sources.items():
            record = self.oracle[name]
            self.assertEqual(model.parameters(record), dict(color=source_colors[pin['registry']['csvid']], reduction=1, printed_mana='{2}'))
            for text in (record['oracle_text'].replace('{1}', '{2}'), record['oracle_text'].replace('you cast', 'each player casts'),
                         record['oracle_text'] + '\nDraw a card.'):
                with self.assertRaises(ValueError):
                    model.parameters(dict(record, oracle_text=text))

    def test_matching_payment_changes_exactly_one_generic(self):
        for kind, printed_generic, color, pips in [('Emerald', 5, 3, 1), ('Jet', 3, 1, 2), ('Pearl', 2, 5, 2),
                                                   ('Ruby', 2, 4, 2), ('Sapphire', 2, 2, 1)]:
            for player in (0, 1):
                with self.subTest(kind=kind, player=player):
                    steps = self.case(kind, f'matching_surplus_{player}')['steps']
                    self.assertIn(dict(op='set_mana', player=player, color=0, value=printed_generic + 1), steps)
                    self.assertIn(dict(op='set_mana', player=player, color=color, value=pips), steps)
                    self.assertIn(dict(op='cast', ref='spell', value=1), steps)
                    resolved = steps.index(dict(op='resolve'))
                    self.assertEqual(self.values(steps[:resolved], 'assert_mana', player=player, color=0), [2])
                    self.assertEqual(self.values(steps[:resolved], 'assert_mana', player=player, color=color), [0])

    def test_wrong_color_opponent_and_hand_are_full_price(self):
        for short, full, payer, cost in [('wrong_color_short_0', 'wrong_color_full_0', 0, 4),
                                          ('opponent_short_0', 'opponent_full_0', 1, 6),
                                          ('source_in_hand_short_0', 'source_in_hand_0', 0, 6)]:
            steps = self.case('Emerald', short)['steps']
            self.assertIn(dict(op='cast', ref='spell', value=0), steps)
            self.assertNotIn(dict(op='resolve'), steps)
            self.assertEqual(self.values(steps, 'assert_mana_total', player=payer)[-1], cost - 1)
            self.assertEqual(self.values(steps, 'assert_card', ref='spell', property='in_hand'), [1])
            steps = self.case('Emerald', full)['steps']
            self.assertIn(dict(op='cast', ref='spell', value=1), steps)
            self.assertEqual(self.values(steps, 'assert_mana_total', player=payer)[-1], 0)

    def test_multicolor_counts_each_matching_source_once(self):
        for variant, generic, names in [('multicolored_one_0', 1, ['Ruby Medallion']),
                                        ('multicolored_both_0', 0, ['Ruby Medallion', 'Emerald Medallion'])]:
            case = self.case('Ruby', variant)
            self.assertEqual([item['name'] for item in case['setup'] if item['name'].endswith(' Medallion')], names)
            self.assertIn(dict(name='Ghor-Clan Rampager', ref='spell', player=0, zone='hand'), case['setup'])
            self.assertIn(dict(op='set_mana', player=0, color=0, value=generic), case['steps'])
            self.assertEqual(self.values(case['steps'], 'assert_mana_total', player=0)[-1], 0)

    def test_stackable_discounts_stop_at_generic_zero(self):
        case = self.case('Jet', 'generic_floor_1')
        self.assertEqual(sum(item['name'] == 'Jet Medallion' for item in case['setup']), 4)
        self.assertIn(dict(op='set_mana', player=1, color=0, value=0), case['steps'])
        self.assertIn(dict(op='set_mana', player=1, color=1, value=2), case['steps'])
        self.assertIn(dict(op='cast', ref='spell', value=1), case['steps'])
        cheap = self.case('Jet', 'no_printed_generic_1')['steps']
        self.assertIn(dict(op='set_mana', player=1, color=1, value=1), cheap)
        short = self.case('Jet', 'missing_colored_mana_1')['steps']
        self.assertIn(dict(op='cast', ref='spell', value=0), short)
        self.assertGreater(self.values(short, 'assert_mana_total', player=1)[-1], 2)
        neutral = self.case('Jet', 'source_colorless_1')['steps']
        self.assertIn(dict(op='set_mana', player=1, color=0, value=2), neutral)
        self.assertIn(dict(op='cast', ref='source', value=1), neutral)

    def test_source_actual_entry_removal_and_countered_payment(self):
        added = self.case('Sapphire', 'after_source_cast_1')['steps']
        self.assertLess(added.index(dict(op='cast', ref='source', value=1)), added.index(dict(op='cast', ref='spell', value=1)))
        self.assertEqual(self.values(added, 'assert_mana_total', player=1), [2, 2, 0, 0])
        removed = self.case('Sapphire', 'remove_source_1')['steps']
        remove = removed.index(dict(op='cast', ref='remove', value=1))
        cast = removed.index(dict(op='cast', ref='spell', value=1))
        self.assertIn(dict(op='choose_target', player=0, ref='source'), removed[:remove])
        self.assertEqual(self.values(removed[remove:cast], 'assert_card', ref='source', property='in_play'), [0])
        self.assertEqual(self.values(removed[cast:], 'assert_mana_total', player=1), [0, 0])
        countered = self.case('Sapphire', 'countered_cast_1')['steps']
        self.assertIn(dict(op='cast_response', ref='counter', value=1), countered)
        self.assertEqual(self.values(countered, 'assert_card', ref='spell', property='in_play'), [0])
        self.assertEqual(self.values(countered, 'assert_mana_total', player=1)[-1], 0)

    def test_noncreature_actual_resolution_and_pending_replay(self):
        steps = self.case('Sapphire', 'noncreature_spell_0')['steps']
        self.assertIn(dict(op='set_mana', player=0, color=0, value=1), steps)
        self.assertEqual(self.values(steps, 'assert_zone', player=0, zone='library')[-1], 5)
        steps = self.case('Ruby', 'noncreature_spell_0')['steps']
        self.assertEqual(self.values(steps, 'assert_mana', player=0, color=4), [0, 5])
        steps = self.case('Emerald', 'pending_snapshot_1')['steps']
        cut = steps.index(dict(op='capture'))
        self.assertEqual(steps.count(dict(op='cast', ref='spell', value=1)), 1)
        self.assertEqual(steps[cut:].count(dict(op='resolve')), 2)
        self.assertIn(dict(op='assert_recorded_state'), steps)


if __name__ == '__main__':
    unittest.main()
