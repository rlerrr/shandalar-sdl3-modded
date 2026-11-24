import copy
import json
import unittest
from pathlib import Path

from generate_cases import registry
import entry_explore_recipes as model


class EntryExploreTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle = json.loads((Path(__file__).parent / 'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {row['name']: row for row in registry()}

    def case(self, variant, name='Merfolk Branchwalker'):
        record = self.oracle[name]
        declaration = dict(family=model.FAMILY, version=2, params=model.classify(record),
                           oracle_text=record['oracle_text'], variants=[variant])
        return model.expand(name, self.rows[name]['csvid'], declaration, model.spec(declaration, record),
                            [variant], self.rows, self.oracle)[0]

    @staticmethod
    def value(steps, op, **fields):
        return next(step['value'] for step in reversed(steps) if step['op'] == op
                    and all(step.get(key) == value for key, value in fields.items()))

    def count(self, steps, who, zone, name):
        return self.value(steps, 'assert_zone_multiplicity', player=who, zone=zone, index=self.rows[name]['csvid'])

    @staticmethod
    def dialogs(steps):
        return [(step['player'], step['value']) for step in steps if step['op'] == 'choose_dialog']

    def test_exact_live_primaries_and_witnesses(self):
        model.validate_witnesses(self.oracle, self.rows)
        self.assertEqual({self.rows[name]['csvid'] for name in model.pins()['sources']}, {17442, 17466, 17518, 17565})
        rows = copy.deepcopy(self.rows)
        rows['Unsummon']['handler'] = 'card_dummy'
        with self.assertRaises(ValueError):
            model.validate_witnesses(self.oracle, rows)

    def test_exact_whole_clause_and_supported_source_cost(self):
        card = self.oracle['Merfolk Branchwalker']
        for text in (model.CLAUSE + '\nFlying', model.CLAUSE.replace('it explores', 'it may explore'),
                     model.CLAUSE.replace('your library', "target opponent's library")):
            with self.assertRaises(ValueError):
                model.classify(dict(card, oracle_text=text))
        with self.assertRaises(ValueError):
            model.classify(dict(card, mana_cost='{X}{G}'))

    def test_land_only_acknowledgement_and_exact_movement(self):
        for who in (0, 1):
            for mode, land in [('land', 'Forest'), ('artifact_land', 'Darksteel Citadel'), ('only_land', 'Forest')]:
                steps = self.case(f'{mode}_{who}')['steps']
                self.assertEqual(self.dialogs(steps), [(who, 0)] * 2)
                self.assertEqual(self.count(steps, who, 'hand', land), 1)
                self.assertEqual(self.count(steps, who, 'graveyard', land), 0)
                self.assertEqual(self.value(steps, 'assert_card', ref='source', property='plus1_counters'), 0)
                self.assertEqual(self.value(steps, 'assert_zone', player=who, zone='library'), 0 if mode == 'only_land' else len(model.SUFFIX))

    def test_nonland_menu_only_after_acknowledgement(self):
        for who in (0, 1):
            for mode, mill in [('nonland_keep', False), ('nonland_mill', True)]:
                steps = self.case(f'{mode}_{who}')['steps']
                self.assertEqual(self.dialogs(steps), [(who, 0), (who, int(mill))] * 2)
                self.assertEqual(self.count(steps, who, 'graveyard', model.DEFAULT_TOP), int(mill))
                self.assertEqual(self.value(steps, 'assert_zone_card', player=who, zone='library', index=0),
                                 self.rows[model.SUFFIX[0] if mill else model.DEFAULT_TOP]['csvid'])
                self.assertEqual(self.value(steps, 'assert_card', ref='source', property='plus1_counters'), 1)
                self.assertEqual(self.value(steps, 'assert_card', ref='source', property='power'), 3)

    def test_empty_library_still_puts_counter_without_showing_dialog(self):
        for name in model.pins()['sources']:
            for who in (0, 1):
                steps = self.case(f'empty_{who}', name)['steps']
                self.assertEqual(self.dialogs(steps), [])
                self.assertEqual(self.value(steps, 'assert_zone', player=who, zone='library'), 0)
                self.assertEqual(self.value(steps, 'assert_life', player=who), 20)
                self.assertEqual(self.value(steps, 'assert_card', ref='source', property='plus1_counters'), 1)

    def test_denied_and_countered_casts_cannot_explore(self):
        for who in (0, 1):
            for mode in ('countered', 'wrong_color', 'insufficient'):
                steps = self.case(f'{mode}_{who}')['steps']
                self.assertEqual(self.dialogs(steps), [])
                self.assertEqual(self.count(steps, who, 'battlefield', 'Merfolk Branchwalker'), 0)
                self.assertEqual(self.count(steps, who, 'graveyard', 'Merfolk Branchwalker'), int(mode == 'countered'))
                self.assertEqual(self.count(steps, who, 'hand', 'Merfolk Branchwalker'), int(mode != 'countered'))
                self.assertEqual(self.value(steps, 'assert_zone_card', player=who, zone='library', index=0), self.rows[model.DEFAULT_TOP]['csvid'])
                self.assertEqual(any(step['op'] == 'cast_response' for step in steps), mode == 'countered')

    def test_exact_mana_and_pending_snapshot_show_no_early_explore(self):
        for name in model.pins()['sources']:
            who = 1
            steps = self.case(f'pending_replay_{who}', name)['steps']
            cast, capture, record, restore = [next(i for i, step in enumerate(steps) if step['op'] == op)
                                               for op in ('cast', 'capture', 'record_state', 'restore')]
            params = model.classify(self.oracle[name])
            self.assertEqual([self.value(steps[:cast], 'set_mana', player=who, color=color) for color in range(6)], params['mana'])
            self.assertEqual([self.value(steps[:capture], 'assert_mana', player=who, color=color) for color in range(6)], [0] * 6)
            self.assertEqual(self.value(steps[:capture], 'assert_stack'), 1)
            self.assertFalse(any(step['op'] == 'choose_dialog' for step in steps[:capture]))
            self.assertEqual(steps[capture+1:record], steps[restore+1:-1])

    def test_blink_gets_new_counter_set_and_entry_from_same_or_new_top(self):
        for who in (0, 1):
            first_mill = self.case(f'blink_mill_land_{who}')['steps']
            first_keep = self.case(f'blink_keep_mill_{who}')['steps']
            self.assertIn(dict(op='bind_permanent', ref='source', player=who, value=17466), first_mill)
            self.assertEqual(self.dialogs(first_mill), [(who, 0), (who, 1), (who, 0)] * 2)
            self.assertEqual(self.dialogs(first_keep), [(who, 0), (who, 0), (who, 0), (who, 1)] * 2)
            self.assertEqual(self.value(first_mill, 'assert_card', ref='source', property='plus1_counters'), 0)
            self.assertEqual(self.value(first_keep, 'assert_card', ref='source', property='plus1_counters'), 1)
            self.assertEqual(self.count(first_mill, who, 'exile', 'Merfolk Branchwalker'), 0)

    def test_earned_counter_stays_through_cleanup(self):
        steps = self.case('cleanup_0')['steps']
        self.assertTrue(any(step['op'] == 'cleanup' for step in steps))
        self.assertEqual(self.value(steps, 'assert_card', ref='source', property='plus1_counters'), 1)

    def test_real_grave_entry_and_bounce_recast(self):
        for who in (0, 1):
            grave = self.case(f'grave_entry_{who}')['steps']
            self.assertIn(dict(op='choose_zone_card', player=who, value=17466), grave)
            self.assertTrue(all(step.get('ref') == 'support' for step in grave if step['op'] == 'cast'))
            self.assertEqual(self.count(grave, who, 'graveyard', 'Unearth'), 1)
            self.assertEqual(self.count(grave, who, 'battlefield', 'Merfolk Branchwalker'), 1)
            self.assertEqual(self.count(grave, who, 'hand', 'Forest'), 1)
            bounce = self.case(f'bounce_recast_{who}')['steps']
            self.assertEqual(sum(step['op'] == 'cast' and step.get('ref') == 'source' for step in bounce), 4)
            self.assertIn(dict(op='bind_hand', ref='source', player=who, value=17466), bounce)
            self.assertEqual(self.count(bounce, who, 'graveyard', 'Unsummon'), 1)

    def test_two_actual_entries_get_independent_counters_and_pay_both_costs(self):
        case = self.case('two_entries_1', "Tishana's Wayfinder")
        steps = case['steps']
        self.assertEqual(self.value(steps, 'set_mana', player=1, color=0), 4)
        self.assertEqual(self.value(steps, 'set_mana', player=1, color=3), 2)
        self.assertEqual(self.count(steps, 1, 'battlefield', case['card']), 2)
        for ref in ('source', 'second'):
            self.assertEqual(self.value(steps, 'assert_card', ref=ref, property='plus1_counters'), 1)
        self.assertEqual(self.count(steps, 1, 'graveyard', model.DEFAULT_TOP), 1)

    def test_exact_bounce_budget_funds_both_casts_and_unsummon(self):
        for name in model.pins()['sources']:
            printed = model.classify(self.oracle[name])['mana']
            with_unsummon = list(printed)
            with_unsummon[2] += 1
            seeded = [combined + cost for combined, cost in zip(with_unsummon, printed)]
            for who in (0, 1):
                steps = self.case(f'bounce_recast_{who}', name)['steps']
                record = next(i for i, s in enumerate(steps) if s['op'] == 'record_state')
                first, support, recast = [i for i, s in enumerate(steps[:record]) if s['op'] == 'cast']
                self.assertEqual([self.value(steps[:first], 'set_mana', player=who, color=c) for c in range(6)], seeded)
                self.assertEqual([self.value(steps[first:support], 'assert_mana', player=who, color=c) for c in range(6)], with_unsummon)
                self.assertEqual([self.value(steps[support:recast], 'assert_mana', player=who, color=c) for c in range(6)], printed)
                self.assertEqual([self.value(steps[recast:record], 'assert_mana', player=who, color=c) for c in range(6)], [0] * 6)


if __name__ == '__main__':
    unittest.main()
