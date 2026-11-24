import json
import unittest
from pathlib import Path

from generate_cases import registry
from upkeep_life_tap_recipes import FAMILY, classify, expand, profiles, spec, validate_witnesses, variants


class UpkeepLifeTapTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle = json.loads((Path(__file__).parent / 'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {row['name']: row for row in registry()}

    def case(self, name, mode, player=0):
        record = self.oracle[name]
        selected = [f'{mode}_{player}']
        declaration = dict(family=FAMILY, version=2, params=classify(record), oracle_text=record['oracle_text'],
                           variants=selected)
        case, = expand(name, self.rows[name]['csvid'], declaration, spec(declaration, record), selected,
                       self.rows, self.oracle)
        self.assertFalse(any('state' in card for card in case['setup']))
        self.assertFalse(any(s['op'] in ('move', 'destroy', 'tap', 'draw') for s in case['steps']))
        return case['steps']

    def actions(self, steps, op, **fields):
        return [step for step in steps if step['op'] == op and all(step.get(k) == v for k, v in fields.items())]

    def values(self, steps, op, **fields):
        return [step['value'] for step in self.actions(steps, op, **fields)]

    def copies(self, steps, name, player, zone):
        return self.values(steps, 'assert_zone_multiplicity', index=self.rows[name]['csvid'], player=player, zone=zone)

    def test_exact_complete_oracle_and_effect_modes(self):
        validate_witnesses(self.oracle, self.rows)
        self.assertEqual({self.rows[name]['csvid'] for name in profiles()['sources']}, {3465, 8653, 5179, 7020})
        for name in profiles()['sources']:
            record = self.oracle[name]
            params = classify(record)
            declaration = dict(family=FAMILY, version=2, params=params, oracle_text=record['oracle_text'],
                               variants=variants(params))
            self.assertEqual(spec(declaration, record), dict(params, family=FAMILY))
            with self.assertRaises(ValueError):
                spec(dict(declaration, params=dict(params, life=float(params['life']))), record)
            with self.assertRaises(ValueError):
                classify(dict(record, oracle_text=record['oracle_text'] + '\nFlying'))
        with self.assertRaises(ValueError):
            classify(self.oracle['Season of the Witch'])

    def test_real_upkeep_affects_that_controller_attack(self):
        for player in (0, 1):
            for name, cost, attack_power in (('Carnophage', 1, 2), ('Sangrophage', 2, 3)):
                for mode, paid in (('attack_paid', True), ('attack_declined', False)):
                    steps = self.case(name, mode, player)
                    first, own = self.actions(steps, 'upkeep_step')
                    choice, = self.actions(steps, 'choose_dialog', player=player)
                    self.assertEqual([first['player'], own['player']], [1-player, player])
                    self.assertLess(steps.index(first), steps.index(choice))
                    self.assertLess(steps.index(choice), steps.index(own))
                    self.assertEqual(choice['value'], int(not paid))
                    self.assertEqual(self.values(steps, 'assert_life', player=player)[-1], 20-cost*paid)
                    self.assertEqual(self.values(steps, 'assert_life', player=1-player)[-1], 20-attack_power*paid)
                    self.assertEqual(self.values(steps, 'attack', ref='source'), [int(paid)])

    def test_actual_aura_keeps_tapped_but_oracle_still_allows_payment(self):
        for player in (0, 1):
            for name, fee in (('Carnophage', 1), ('Sangrophage', 2)):
                steps = self.case(name, 'already_tapped', player)
                attach, = self.actions(steps, 'cast', ref='aura', value=1)
                first, second = self.actions(steps, 'untap_step')
                self.assertLess(steps.index(first), steps.index(attach))
                self.assertLess(steps.index(attach), steps.index(second))
                self.assertEqual(self.copies(steps, 'Claustrophobia', 1-player, 'battlefield')[-1], 1)
                self.assertEqual(self.values(steps, 'choose_dialog', player=player), [0])
                self.assertEqual(self.values(steps, 'assert_card', ref='source', property='tapped')[-1], 1)
                self.assertEqual(self.values(steps, 'assert_life', player=player)[-1], 20-fee)

    def test_life_payment_lock_shortage_and_exact_boundary(self):
        for player in (0, 1):
            for name, fee in (('Carnophage', 1), ('Sangrophage', 2), ('Vile Consumption', 1)):
                exact = self.case(name, 'exact_life', player)
                self.assertEqual(self.values(exact, 'set_life', player=player), [fee])
                self.assertEqual(self.values(exact, 'assert_life', player=player)[-1], 0)
                locked = self.case(name, 'locked_life', player)
                self.assertFalse(self.actions(locked, 'choose_dialog'))
                self.assertEqual(self.values(locked, 'assert_life', player=player)[-1], 20)
                if name == 'Vile Consumption':
                    self.assertEqual(self.copies(locked, 'Platinum Emperion', player, 'graveyard')[-1], 1)
                else:
                    self.assertEqual(self.values(locked, 'assert_card', ref='source', property='tapped')[-1], 1)
            low = self.case('Sangrophage', 'unaffordable', player)
            self.assertFalse(self.actions(low, 'choose_dialog'))
            self.assertEqual(self.values(low, 'assert_life', player=player)[-1], 1)
            self.assertEqual(self.values(low, 'assert_card', ref='source', property='tapped')[-1], 1)

    def test_vile_taxes_each_actual_upkeep_and_both_identical_choices(self):
        for player in (0, 1):
            steps = self.case('Vile Consumption', 'opponent_upkeep', player)
            self.assertEqual(self.values(steps, 'choose_dialog', player=1-player), [0])
            self.assertEqual(self.values(steps, 'assert_life', player=1-player)[-1], 19)
            self.assertEqual(self.values(steps, 'assert_life', player=player)[-1], 20)
            mixed = self.case('Vile Consumption', 'mixed', player)
            self.assertEqual(self.values(mixed, 'choose_dialog', player=player), [0, 1])
            self.assertEqual(self.copies(mixed, 'Grizzly Bears', player, 'battlefield')[-1], 1)
            self.assertEqual(self.copies(mixed, 'Grizzly Bears', player, 'graveyard')[-1], 1)
            self.assertEqual(self.values(mixed, 'assert_life', player=player)[-1], 19)
            empty = self.case('Vile Consumption', 'no_creature', player)
            self.assertFalse(self.actions(empty, 'choose_dialog'))

    def test_bringer_printed_normal_rainbow_and_actual_selection(self):
        for player in (0, 1):
            for mode, choice, remaining in (('choose_rainbow', 0, [7, 1, 0, 0, 0, 0]),
                                            ('choose_normal', 1, [0, 0, 1, 1, 1, 1])):
                steps = self.case('Bringer of the Black Dawn', mode, player)
                self.assertEqual(self.values(steps, 'choose_dialog', player=player), [choice])
                self.assertEqual([self.values(steps, 'assert_mana', player=player, color=color)[-1]
                                  for color in range(6)], remaining)
            exact = self.case('Bringer of the Black Dawn', 'rainbow', player)
            self.assertFalse(self.actions(exact, 'choose_dialog'))
            self.assertEqual(self.copies(exact, 'Bringer of the Black Dawn', player, 'battlefield')[-1], 1)

    def test_bringer_optional_payment_preserves_hand_and_reorders_real_library(self):
        for player in (0, 1):
            for mode, shuffles, top in (('pay', 1, 'Plains'), ('repeat', 2, 'Mountain'), ('decline', 0, 'Island')):
                steps = self.case('Bringer of the Black Dawn', mode, player)
                self.assertEqual(self.values(steps, 'assert_shuffle_count', player=player)[-1], shuffles)
                self.assertEqual(self.values(steps, 'assert_shuffle_count', player=1-player)[-1], 0)
                self.assertEqual(self.values(steps, 'assert_zone_card', player=player, zone='library', index=0)[-1],
                                 self.rows[top]['csvid'])
                self.assertEqual(self.values(steps, 'assert_zone', player=player, zone='hand')[-1], 0)
                expected_life = 6 if mode == 'decline' and player == 1 else 20-2*shuffles
                self.assertEqual(self.values(steps, 'assert_life', player=player)[-1], expected_life)
            required = self.case('Bringer of the Black Dawn', 'search_policy', player)
            self.assertEqual(self.values(required, 'choose_optional_zone', player=player), [self.rows['Plains']['csvid']])
            self.assertEqual(self.values(required, 'assert_forced_zone_choices'), [1])

    def test_bringer_actual_trample_and_vile_paid_attack(self):
        for player in (0, 1):
            trample = self.case('Bringer of the Black Dawn', 'trample', player)
            self.assertEqual(self.values(trample, 'block', ref='blocker'), [1])
            self.assertEqual(self.values(trample, 'assert_life', player=1-player)[-1], 16)
            self.assertEqual(self.copies(trample, "Mons's Goblin Raiders", 1-player, 'graveyard')[-1], 1)
            self.assertEqual(self.values(trample, 'assert_card', ref='source', property='damage')[-1], 1)
            paid = self.case('Vile Consumption', 'paid_attack', player)
            self.assertEqual(self.values(paid, 'attack', ref='ally'), [1])
            self.assertEqual(self.values(paid, 'assert_life', player=1-player)[-1], 18)

    def test_countered_source_never_offers_payment_and_real_upkeep_replays(self):
        for name in profiles()['sources']:
            counter = self.case(name, 'countered')
            self.assertEqual(self.values(counter, 'cast_response', ref='counter'), [1])
            for op in ('choose_dialog', 'choose_trigger_source', 'choose_zone_card'):
                self.assertFalse(self.actions(counter, op))
            self.assertEqual(self.copies(counter, name, 0, 'graveyard')[-1], 1)
            snap = self.case(name, 'snapshot')
            ops = [step['op'] for step in snap]
            self.assertEqual(snap[ops.index('capture')+1:ops.index('record_state')],
                             snap[ops.index('restore')+1:ops.index('assert_recorded_state')])
