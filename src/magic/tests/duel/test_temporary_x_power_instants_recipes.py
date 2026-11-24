import json
import unittest
from pathlib import Path

from generate_cases import registry
from temporary_x_power_instants_recipes import FAMILY, classify, expand, profiles, spec, validate_witnesses, variants


class TemporaryXPowerInstantsTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle = json.loads((Path(__file__).parent / 'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {card['name']: card for card in registry()}

    def case(self, name, mode, actor=0):
        record = self.oracle[name]
        selected = [f'{mode}_{actor}']
        declaration = dict(family=FAMILY, version=2, params=classify(record), oracle_text=record['oracle_text'],
                           variants=selected)
        case, = expand(name, self.rows[name]['csvid'], declaration, spec(declaration, record), selected,
                       self.rows, self.oracle)
        self.assertFalse(any('state' in setup for setup in case['setup']))
        self.assertFalse(any(s['op'] in ('activate', 'destroy', 'move', 'draw', 'discard') for s in case['steps']))
        return case['steps']

    def actions(self, steps, op, **fields):
        return [s for s in steps if s['op'] == op and all(s.get(key) == value for key, value in fields.items())]

    def values(self, steps, op, **fields):
        return [s['value'] for s in self.actions(steps, op, **fields)]

    def copies(self, steps, name, player, zone):
        return self.values(steps, 'assert_zone_multiplicity', index=self.rows[name]['csvid'], player=player, zone=zone)

    def test_exact_whole_pricing_scope_and_card_bindings(self):
        validate_witnesses(self.oracle, self.rows)
        self.assertEqual({self.rows[name]['csvid'] for name in profiles()['sources']}, {113, 1030, 3590, 8337, 12455, 3498})
        for name in profiles()['sources']:
            record = self.oracle[name]
            params = classify(record)
            declaration = dict(family=FAMILY, version=2, params=params, oracle_text=record['oracle_text'],
                               variants=variants(params))
            self.assertEqual(spec(declaration, record), dict(params, family=FAMILY))
            with self.assertRaises(ValueError):
                spec(dict(declaration, params=dict(params, instant=int(params['instant']))), record)
            with self.assertRaises(ValueError):
                classify(dict(record, oracle_text=record['oracle_text'] + '\nDraw a card.'))
        self.assertNotIn('opponent_turn_0', variants(classify(self.oracle['Bloodcurdling Scream'])))

    def test_same_oracle_aliases_choose_exact_x_before_actual_target(self):
        for player in (0, 1):
            for name in ('Howl from Beyond', 'Enrage', 'Bloodcurdling Scream'):
                for mode, x in (('exact', 4), ('zero', 0)):
                    steps = self.case(name, mode, player)
                    declared, = self.actions(steps, 'choose_x', player=player)
                    target, = self.actions(steps, 'choose_target', player=player, ref='recipient')
                    self.assertEqual(declared['value'], x)
                    self.assertLess(steps.index(declared), steps.index(target))
                    self.assertEqual(self.values(steps, 'assert_card', ref='recipient', property='power')[-2:], [3, 3+x])
                    self.assertEqual(self.values(steps, 'assert_card', ref='recipient', property='toughness')[-1], 3)
                    self.assertEqual(self.copies(steps, name, player, 'graveyard')[-1], 1)

    def test_mana_reserve_and_distinct_values_of_repeated_casts(self):
        for player in (0, 1):
            surplus = self.case('Enrage', 'reserve', player)
            self.assertEqual(self.values(surplus, 'assert_mana', player=player, color=0)[-1], 2)
            repeated = self.case('Enrage', 'repeated', player)
            self.assertEqual(self.values(repeated, 'choose_x', player=player), [1, 4])
            self.assertEqual(self.values(repeated, 'assert_card', ref='recipient', property='power')[-3:], [4, 4, 8])
            self.assertEqual(self.copies(repeated, 'Enrage', player, 'graveyard')[-1], 2)

    def test_cast_failure_preserves_spell_hand_and_target(self):
        for player in (0, 1):
            for name in profiles()['sources']:
                for test in ('wrong_color', 'short', 'no_legal_target'):
                    steps = self.case(name, test, player)
                    self.assertEqual(self.values(steps, 'cast', ref='source'), [0])
                    for op in ('choose_target', 'choose_x', 'choose_number'):
                        self.assertFalse(self.actions(steps, op))
                    self.assertEqual(self.copies(steps, name, player, 'hand')[-1], 1)

    def test_real_response_prevents_boost_but_preserves_paid_costs(self):
        for player in (0, 1):
            for test in ('countered', 'lost_target'):
                steps = self.case('Hatred', test, player)
                self.assertTrue(self.actions(steps, 'announce_spell', ref='source'))
                self.assertEqual(self.values(steps, 'cast_response', ref='response'), [1])
                self.assertTrue(self.actions(steps, 'resolve_response'))
                self.assertEqual(self.values(steps, 'assert_life', player=player)[-1], 16)
                self.assertEqual(self.values(steps, 'assert_card', ref='recipient', property='power')[-1], 3)
                if test == 'lost_target':
                    self.assertEqual(self.copies(steps, 'Hill Giant', player, 'hand')[-1], 1)

    def test_actual_combat_opposing_target_and_expiration(self):
        for player in (0, 1):
            for mode, expected_life in (('combat', 13), ('blocked', 20), ('opponent_target', 20), ('expiry', 20)):
                steps = self.case('Bloodcurdling Scream', mode, player)
                self.assertEqual(self.values(steps, 'assert_life', player=1-player)[-1], expected_life)
                self.assertEqual(self.values(steps, 'assert_card', ref='recipient', property='power')[-1],
                                 3 if mode == 'expiry' else 7)
                self.assertEqual(self.values(steps, 'assert_card', ref='unaffected', property='power')[-1], 3)
                if mode in ('combat', 'blocked'):
                    cast, = self.actions(steps, 'cast', ref='source')
                    attack, = self.actions(steps, 'attack', ref='recipient')
                    self.assertLess(steps.index(cast), steps.index(attack))
                if mode == 'blocked':
                    self.assertEqual(self.copies(steps, 'Fortified Rampart', 1-player, 'graveyard')[-1], 1)

    def test_hatred_real_life_cost_order_and_zero(self):
        for test, paid in (('exact', 4), ('zero', 0)):
            steps = self.case('Hatred', test, 0)
            target, = self.actions(steps, 'choose_target', ref='recipient')
            choice, = self.actions(steps, 'choose_number', player=0)
            self.assertLess(steps.index(target), steps.index(choice))
            self.assertEqual(choice['value'], paid)
            self.assertEqual(self.values(steps, 'assert_life', player=0)[-1], 20-paid)
            ai = self.case('Hatred', test, 1)
            self.assertFalse(self.actions(ai, 'choose_number'))
            self.assertEqual(self.values(ai, 'set_life', player=0), [3+paid])
            self.assertEqual(self.values(ai, 'assert_life', player=1)[-1], 20-paid)

    def test_rush_reads_actual_response_modified_power(self):
        for player in (0, 1):
            steps = self.case('Rush of Blood', 'resolution_power', player)
            self.assertEqual(self.values(steps, 'cast_response', ref='response'), [1])
            self.assertFalse(self.actions(steps, 'choose_x'))
            self.assertEqual(self.values(steps, 'assert_card', ref='recipient', property='power')[-1], 12)
            self.assertEqual(self.values(steps, 'assert_card', ref='recipient', property='toughness')[-1], 6)
            zero = self.case('Rush of Blood', 'zero', player)
            self.assertEqual(self.values(zero, 'assert_card', ref='recipient', property='power')[-1], 0)

    def test_rage_actual_attack_and_next_turn_upkeep_exact_once(self):
        for player in (0, 1):
            for mode, lost in (('exact', False), ('opponent_target', False), ('lost_target', True), ('countered', True)):
                steps = self.case('Balduvian Rage', mode, player)
                attack, = self.actions(steps, 'attack', ref='recipient')
                cast, = self.actions(steps, 'cast', ref='source')
                first, second = self.actions(steps, 'upkeep_step')
                self.assertLess(steps.index(attack), steps.index(cast))
                self.assertEqual(first['player'], player if mode == 'opponent_target' else 1-player)
                self.assertEqual(second['player'], 1-first['player'])
                self.assertTrue(all(v == 3 for v in self.values(steps[:steps.index(first)], 'assert_zone',
                                                                  player=player, zone='library')))
                self.assertEqual(self.values(steps, 'assert_zone', player=player, zone='library')[-1], 3 if lost else 2)
                self.assertEqual(self.values(steps, 'assert_zone', player=1-player, zone='library')[-1], 3)

    def test_pending_replay_is_exact(self):
        for name in profiles()['sources']:
            steps = self.case(name, 'snapshot', 1)
            ops = [s['op'] for s in steps]
            self.assertLess(ops.index('cast'), ops.index('capture'))
            self.assertEqual(steps[ops.index('capture')+1:ops.index('record_state')],
                             steps[ops.index('restore')+1:ops.index('assert_recorded_state')])
