import json
import unittest
from pathlib import Path

from depletion_dual_land_recipes import FAMILY, classify, expand, profiles, spec, validate_witnesses, variants
from generate_cases import registry
from recipes import COLORS


class DepletionDualLandRecipesTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle = json.loads((Path(__file__).parent / 'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {row['name']: row for row in registry()}

    def declaration(self, name, selection):
        record = self.oracle[name]
        return dict(family=FAMILY, version=2, oracle_text=record['oracle_text'], params=classify(record), variants=selection)

    def case(self, name, variant):
        declaration = self.declaration(name, [variant])
        case, = expand(name, self.rows[name]['csvid'], declaration, spec(declaration, self.oracle[name]),
                       [variant], self.rows, self.oracle)
        return case

    def values(self, steps, op, key='value', **fields):
        return [step[key] for step in steps if step['op'] == op and all(step.get(k) == v for k, v in fields.items())]

    def phases(self, steps, phase, ref='source'):
        rows = []
        for index, step in enumerate(steps):
            if step['op'] != phase:
                continue
            tail = steps[index + 1:]
            count = self.values(tail, 'assert_counter', ref=ref, counter_kind='depletion')[0]
            tapped = self.values(tail, 'assert_card', ref=ref, property='tapped')[0]
            rows.append((step['player'], count, tapped))
        return rows

    def test_whole_oracle_pins_and_distinct_typed_outputs(self):
        validate_witnesses(self.oracle, self.rows)
        readings = {2012: ['W', 'U'], 2015: ['B', 'R'], 2082: ['U', 'B'], 2132: ['R', 'G'], 2143: ['G', 'W']}
        self.assertEqual({self.rows[name]['csvid'] for name in profiles()['sources']}, set(readings))
        for name in profiles()['sources']:
            record = self.oracle[name]
            params = classify(record)
            self.assertEqual(params['outputs'], readings[self.rows[name]['csvid']])
            self.assertEqual(len(set(variants(params))), 30)
            declaration = self.declaration(name, ['ordinary_entry_0'])
            spec(declaration, record)
            with self.assertRaises(ValueError):
                spec(dict(declaration, params=dict(params, add_depletion=True)), record)
            with self.assertRaises(ValueError):
                classify(dict(record, oracle_text=record['oracle_text'].replace('your upkeep', "each player's upkeep")))

    def test_normal_land_play_uses_one_actual_allowance_and_next_turn(self):
        for owner in (0, 1):
            case = self.case('Land Cap', f'one_land_allowance_{owner}')
            self.assertCountEqual(case['setup'], [dict(name='Land Cap', ref=ref, player=owner, zone='hand') for ref in ('source', 'extra')])
            steps = case['steps']
            self.assertEqual(self.values(steps, 'play_land', ref='source'), [1])
            self.assertEqual(self.values(steps, 'play_land', ref='extra'), [0, 1])
            self.assertEqual(self.values(steps, 'upkeep_step', key='player'), [1-owner, owner])
            self.assertEqual(self.values(steps, 'assert_zone_multiplicity', player=owner, zone='battlefield', index=2012)[-1], 2)
            self.assertFalse(self.values(steps, 'cast', ref='source'))

    def test_each_printed_color_is_produced_and_spent_in_both_seats(self):
        for name in profiles()['sources']:
            colors = classify(self.oracle[name])['outputs']
            for owner in (0, 1):
                for mode, color in zip(('spend_first', 'spend_second'), colors):
                    steps = self.case(name, f'{mode}_{owner}')['steps']
                    self.assertEqual(self.values(steps, 'choose_color', player=owner), [COLORS[color]])
                    self.assertEqual(self.values(steps, 'activate', ref='source'), [1])
                    self.assertEqual(self.values(steps, 'set_mana', player=owner, color=COLORS[color]), [0])
                    self.assertEqual(self.values(steps, 'assert_mana', player=owner, color=COLORS[color])[-2:], [1, 0])
                    self.assertEqual(self.values(steps, 'cast', ref='spend'), [1])
                    self.assertEqual(self.values(steps, 'assert_counter', ref='source', counter_kind='depletion'), [0, 1, 1])

    def test_own_untap_precedes_own_upkeep_and_next_natural_untap(self):
        for owner in (0, 1):
            steps = self.case('River Delta', f'single_upkeep_cycle_{owner}')['steps']
            self.assertEqual(self.phases(steps, 'untap_step'), [(1-owner, 1, 1), (owner, 1, 1), (1-owner, 0, 1), (owner, 0, 0)])
            self.assertEqual(self.phases(steps, 'upkeep_step'), [(1-owner, 1, 1), (owner, 0, 1), (1-owner, 0, 1), (owner, 0, 0)])
            self.assertEqual(self.values(steps, 'activate', ref='source'), [1, 0, 1])
            self.assertEqual(self.values(steps, 'choose_color', player=owner), [COLORS['U'], COLORS['B']])

    def test_twiddle_starts_zero_or_grows_to_two_with_only_one_removal(self):
        zero = self.case('Veldt', 'zero_depletion_untaps_0')['steps']
        self.assertEqual(self.values(zero, 'choose_dialog', player=1), [0])
        self.assertEqual(self.phases(zero, 'untap_step'), [(1, 0, 1), (0, 0, 0)])
        self.assertTrue(all(n == 0 for n in self.values(zero, 'assert_counter', ref='source', counter_kind='depletion')))
        double = self.case('Timberline Ridge', 'two_upkeep_cycles_1')['steps']
        self.assertEqual(self.values(double, 'choose_dialog', player=0), [1])
        self.assertEqual(self.values(double, 'cast', ref='twiddle'), [1])
        self.assertEqual(self.values(double, 'activate', ref='source'), [1, 1, 0, 0])
        self.assertEqual(self.phases(double, 'untap_step'), [(0, 2, 1), (1, 2, 1), (0, 1, 1), (1, 1, 1), (0, 0, 1), (1, 0, 0)])
        self.assertEqual(self.phases(double, 'upkeep_step'), [(0, 2, 1), (1, 1, 1), (0, 1, 1), (1, 0, 1), (0, 0, 1), (1, 0, 0)])

    def test_separate_controller_upkeeps_do_not_share_counters(self):
        steps = self.case('Lava Tubes', 'split_controller_upkeeps_0')['steps']
        self.assertEqual(self.phases(steps, 'upkeep_step'), [(1, 1, 1), (0, 0, 1), (1, 0, 1), (0, 0, 0)])
        self.assertEqual(self.values(steps, 'assert_counter', ref='extra', counter_kind='depletion'), [0, 1, 1, 1, 1, 1, 0, 0, 0])
        self.assertEqual(self.values(steps, 'play_land', ref='extra'), [1])
        self.assertEqual(self.values(steps, 'choose_color', player=1), [COLORS['R']])

    def test_atlas_uses_real_off_turn_entry_and_bounce_resets(self):
        steps = self.case('River Delta', 'opposing_turn_atlas_0')['steps']
        self.assertEqual(self.values(steps, 'play_land', ref='source'), [0])
        self.assertEqual(self.values(steps, 'activate', ref='atlas'), [1])
        self.assertEqual(self.values(steps, 'choose_zone_card', player=0), [2082])
        self.assertEqual(self.values(steps, 'assert_counter', ref='source', counter_kind='depletion'), [0, 1])
        bounce = self.case('River Delta', 'reentry_reset_1')['steps']
        self.assertEqual(self.values(bounce, 'cast', ref='bounce'), [1])
        self.assertEqual(self.values(bounce, 'play_land', ref='source'), [1, 0])
        self.assertEqual(self.values(bounce, 'bind_hand_copy', ref='source'), [2082])
        self.assertEqual(self.values(bounce, 'assert_counter', ref='source', counter_kind='depletion'), [0, 1, 1, 0])

    def test_replay_keeps_real_pending_upkeep_and_atlas_boundaries(self):
        for mode, first in (('atlas_pending_snapshot', 'choose_zone_card'), ('upkeep_ready_snapshot', 'upkeep_step')):
            steps = self.case('Lava Tubes', mode + '_1')['steps']
            names = [step['op'] for step in steps]
            capture, record, restore = [names.index(op) for op in ('capture', 'record_state', 'restore')]
            self.assertEqual(names[capture + 1], first)
            self.assertEqual(steps[capture + 1:record], steps[restore + 1:-1])
            self.assertFalse(any(step['op'] == 'set_mana' for step in steps[capture + 1:record]))
