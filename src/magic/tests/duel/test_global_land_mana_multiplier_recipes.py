import json
import unittest
from pathlib import Path

from generate_cases import registry
from global_land_mana_multiplier_recipes import (
    DAMAGE_RULE, FAMILY, RULE, classify, expand, pins, spec, validate_witnesses, variants,
)
from recipes import COLORS


class GlobalLandManaMultiplierRecipesTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle = json.loads((Path(__file__).parent / 'data/oracle.json').read_bytes())['cards']
        cls.registry = {row['name']: row for row in registry()}

    def declaration(self, name, variant):
        record = self.oracle[name]
        return dict(family=FAMILY, version=2, oracle_text=record['oracle_text'],
                    params=classify(record), variants=[variant])

    def case(self, name, variant):
        d = self.declaration(name, variant)
        case, = expand(name, self.registry[name]['csvid'], d, spec(d, self.oracle[name]),
                       [variant], self.registry, self.oracle)
        return case

    def matching(self, case, op, **fields):
        return [step for step in case['steps'] if step['op'] == op
                and all(step.get(k) == v for k, v in fields.items())]

    def values(self, case, op, **fields):
        return [step['value'] for step in self.matching(case, op, **fields)]

    def wallet(self, case, player):
        return [self.values(case, 'assert_mana', player=player, color=i)[-1] for i in range(6)]

    def test_full_printed_rules_and_typed_pins(self):
        validate_witnesses(self.oracle, self.registry)
        self.assertEqual({pin['registry']['csvid'] for pin in pins()['sources'].values()},
                         {13873, 1248, 155, 5054, 13250})
        for name, expected in [('Mana Flare', RULE), ('Dictate of Karametra', 'Flash\n' + RULE),
                               ('Overabundance', DAMAGE_RULE), ('Zhur-Taa Ancient', RULE)]:
            record = self.oracle[name]
            d = self.declaration(name, 'owner_land_0')
            self.assertEqual(d['oracle_text'], expected)
            self.assertEqual(len(variants(spec(d, record))), 32 if name == 'Mana Flare' else 34)
            with self.assertRaises(ValueError):
                spec(dict(d, version=2.0), record)
            with self.assertRaises(ValueError):
                spec(dict(d, params=dict(d['params'], mana=[float(n) for n in d['params']['mana']])), record)
            with self.assertRaises(ValueError):
                classify(dict(record, oracle_text=expected + '\nDraw a card.'))

    def test_opening_cast_initializes_both_fixture_wallets(self):
        for name in pins()['sources']:
            for variant in variants(classify(self.oracle[name])):
                case = self.case(name, variant)
                first_caster = case['steps'][0]['player']
                inactive, payer = case['steps'][1:7], case['steps'][7:13]
                self.assertEqual(inactive, [
                    dict(op='set_mana', player=1 - first_caster, color=color, value=0)
                    for color in range(6)
                ])
                self.assertEqual([(step['op'], step['player'], step['color']) for step in payer],
                                 [('set_mana', first_caster, color) for color in range(6)])
                self.assertEqual(case['steps'][13]['op'], 'cast')

    def test_actual_land_taps_credit_only_the_land_controller(self):
        for owner in (0, 1):
            for scenario, player in [('owner_land', owner), ('opponent_land', 1 - owner)]:
                c = self.case('Mana Flare', f'{scenario}_{owner}')
                self.assertEqual(self.values(c, 'activate', ref='producer'), [1])
                self.assertEqual(self.wallet(c, player), [0, 0, 0, 2, 0, 0])
                self.assertEqual(self.wallet(c, 1 - player), [0] * 6)

    def test_same_color_multiproduction_adds_exactly_one(self):
        for owner in (0, 1):
            for scenario, color, amount in [('colorless_land', 0, 2), ('two_colorless', 0, 3),
                                           ('selected_white', COLORS['W'], 2)]:
                c = self.case('Heartbeat of Spring', f'{scenario}_{owner}')
                self.assertEqual(self.wallet(c, owner), [amount if i == color else 0 for i in range(6)])
                self.assertEqual(self.values(c, 'choose_color'),
                                 [COLORS['W']] if scenario == 'selected_white' else [])

    def test_true_mixed_outputs_allow_either_legal_extra_and_pin_wallet(self):
        for owner in (0, 1):
            for scenario, player, color in [('mixed_red', owner, 'R'), ('mixed_green', owner, 'G'),
                                            ('opponent_mixed', 1 - owner, 'R')]:
                c = self.case('Mana Flare', f'{scenario}_{owner}')
                self.assertEqual(self.matching(c, 'choose_color'),
                                 [dict(op='choose_color', player=owner if player == owner else -1,
                                       value=COLORS[color])])
                self.assertEqual(self.wallet(c, player),
                                 [0, 0, 0, 1 + (color == 'G'), 1 + (color == 'R'), 0])
                self.assertEqual(self.wallet(c, 1 - player), [0] * 6)

    def test_nonland_mana_and_twiddle_do_not_trigger(self):
        for owner in (0, 1):
            c = self.case('Overabundance', f'nonland_mana_{owner}')
            self.assertEqual(self.wallet(c, owner), [0, 0, 0, 1, 0, 0])
            self.assertEqual(self.wallet(c, 1 - owner), [2, 0, 0, 0, 0, 0])
            self.assertEqual(self.values(c, 'assert_life', player=owner)[-1], 20)
            tap = self.case('Overabundance', f'nonmana_tap_{owner}')
            self.assertEqual(self.matching(tap, 'choose_target', ref='producer', player=owner),
                             [dict(op='choose_target', ref='producer', player=owner)])
            self.assertFalse(self.matching(tap, 'activate'))
            self.assertEqual(self.wallet(tap, owner), [0] * 6)
            self.assertEqual(self.values(tap, 'assert_card', ref='producer', property='tapped')[-1], 1)

    def test_only_active_copies_contribute_and_prevention_keeps_mana(self):
        for owner in (0, 1):
            for scenario, mana, life in [('while_pending', 3, 19), ('source_removed', 3, 19),
                                         ('two_multipliers', 3, 18), ('prevent_damage', 2, 20)]:
                player = 1 - owner if scenario == 'prevent_damage' else owner
                c = self.case('Overabundance', f'{scenario}_{owner}')
                self.assertEqual(self.wallet(c, player)[COLORS['G']], mana)
                self.assertEqual(self.values(c, 'assert_life', player=player)[-1], life)

    def test_spending_requires_actual_land_outputs(self):
        for owner in (0, 1):
            for scenario in ('spend_pool', 'pay_during'):
                c = self.case('Overabundance', f'{scenario}_{owner}')
                self.assertEqual(self.values(c, 'cast', ref='spender'), [1])
                self.assertEqual(self.values(c, 'activate', ref='producer'),
                                 [1] if scenario == 'spend_pool' else [])
                self.assertEqual(len(self.matching(c, 'choose_mana_source', ref='producer')),
                                 int(scenario == 'pay_during' and owner == 0))
                self.assertEqual(self.wallet(c, owner), [0] * 6)
                self.assertEqual(self.values(c, 'assert_life', player=owner)[-1], 19)
                self.assertEqual(self.values(c, 'assert_card', ref='producer', property='tapped')[-1], 1)

    def test_flash_combat_and_replay_use_production_actions(self):
        for owner in (0, 1):
            c = self.case('Dictate of Karametra', f'flash_response_{owner}')
            self.assertIn(dict(op='turn', player=1 - owner, value=0), c['steps'])
            events = [step['op'] for step in c['steps']]
            self.assertLess(events.index('announce_spell'), events.index('cast_response'))
            self.assertLess(events.index('cast_response'), events.index('resolve_response'))
            self.assertLess(events.index('resolve_response'), events.index('activate'))
            self.assertIn(2, self.values(c, 'assert_stack'))
            combat = self.case('Zhur-Taa Ancient', f'creature_combat_{owner}')
            self.assertEqual(self.values(combat, 'attack', ref='source'), [0, 1])
            self.assertEqual(len(self.matching(combat, 'advance_turn')), 2)
            self.assertEqual(self.values(combat, 'assert_life', player=1 - owner)[-1], 13)
            replay = self.case('Mana Flare', f'replay_{owner}')
            self.assertEqual(self.values(replay, 'activate', ref='producer'), [1, 1])
            self.assertEqual(len(self.matching(replay, 'assert_recorded_state')), 1)
