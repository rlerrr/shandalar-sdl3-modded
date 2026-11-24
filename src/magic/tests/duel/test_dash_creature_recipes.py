import copy
import json
import unittest
from pathlib import Path

from generate_cases import registry
import dash_creature_recipes as model


class DashCreatureTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle = json.loads((Path(__file__).parent / 'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {row['name']: row for row in registry()}

    def case(self, name, variant):
        record = self.oracle[name]
        declaration = dict(family=model.FAMILY, version=2, params=model.classify(record),
                           oracle_text=record['oracle_text'], variants=[variant])
        return model.expand(name, self.rows[name]['csvid'], declaration, model.spec(declaration, record),
                            [variant], self.rows, self.oracle)[0]

    @staticmethod
    def last(steps, op, **fields):
        return next(step['value'] for step in reversed(steps) if step['op'] == op
                    and all(step.get(key) == value for key, value in fields.items()))

    def count(self, steps, player, zone, name):
        return self.last(steps, 'assert_zone_multiplicity', player=player, zone=zone, index=self.rows[name]['csvid'])

    def pool(self, steps, op, player):
        return [self.last(steps, op, player=player, color=color) for color in range(6)]

    def test_actual_primary_and_witness_pins(self):
        model.validate_witnesses(self.oracle, self.rows)
        self.assertEqual({self.rows[name]['csvid'] for name in model.pins()['sources']}, {14509, 14585, 14947, 15009})
        invalid = copy.deepcopy(self.rows)
        invalid['Bazaar Trader']['handler'] = 'card_dummy'
        with self.assertRaises(ValueError):
            model.validate_witnesses(self.oracle, invalid)

    def test_independent_fixed_costs_full_text_and_no_additional_mechanics(self):
        card = self.oracle['Mardu Scout']
        model_row = model.classify(card)
        self.assertEqual(model_row['regular'], [0, 0, 0, 0, 2, 0])
        self.assertEqual(model_row['alternate'], [1, 0, 0, 0, 1, 0])
        for text in (card['oracle_text'].replace('its owner', 'its controller'),
                     card['oracle_text'].replace('gains haste', 'gains flying'), card['oracle_text'] + '\nFirst strike'):
            with self.assertRaises(ValueError):
                model.classify(dict(card, oracle_text=text))

    def test_exact_alternate_payments_and_optional_menu_are_real_cast_choices(self):
        for name in model.pins()['sources']:
            card = model.classify(self.oracle[name])
            for who in (0, 1):
                for mode, dash, menu in [('dash_exact', True, name == 'Kolaghan Skirmisher'),
                                         ('normal_exact', False, name != 'Kolaghan Skirmisher')]:
                    steps = self.case(name, f'{mode}_{who}')['steps']
                    first = next(i for i, s in enumerate(steps) if s['op'] == 'cast')
                    resolve = next(i for i, s in enumerate(steps) if s['op'] == 'resolve')
                    self.assertEqual(self.pool(steps[:first], 'set_mana', who), card['alternate' if dash else 'regular'])
                    self.assertEqual([(s['player'], s['value']) for s in steps[:first] if s['op'] == 'choose_dialog'],
                                     [(who, int(not dash))] if menu else [])
                    self.assertEqual(self.pool(steps[first:resolve], 'assert_mana', who), model.ZERO)
                    self.assertEqual(self.last(steps[first:resolve], 'assert_stack'), 1)

    def test_dual_pool_spends_only_selected_fee_and_cancel_spends_none(self):
        for name in model.pins()['sources']:
            params = model.classify(self.oracle[name])
            seed = [max(a, b) for a, b in zip(params['regular'], params['alternate'])]
            for who in (0, 1):
                for mode, fee, choice in [('normal_menu', params['regular'], 1), ('dash_menu', params['alternate'], 0), ('cancel', model.ZERO, 2)]:
                    steps = self.case(name, f'{mode}_{who}')['steps']
                    self.assertIn(dict(op='choose_dialog', player=who, value=choice), steps)
                    self.assertEqual(self.pool(steps, 'assert_mana', who), [n - c for n, c in zip(seed, fee)])
                    self.assertEqual(self.last(steps, 'cast', ref='source'), int(mode != 'cancel'))
                    if mode == 'cancel':
                        self.assertEqual(self.count(steps, who, 'hand', name), 1)

    def test_dashing_attacks_fresh_and_regular_cannot(self):
        name = 'Mardu Scout'
        for who in (0, 1):
            haste = self.case(name, f'dash_combat_{who}')['steps']
            ordinary = self.case(name, f'normal_fresh_attack_{who}')['steps']
            self.assertIn(dict(op='attack', ref='source', value=1), haste)
            self.assertIn(dict(op='attack', ref='source', value=0), ordinary)
            self.assertEqual(self.last(haste, 'assert_life', player=1-who), 17)
            self.assertEqual(self.last(ordinary, 'assert_life', player=1-who), 20)
            self.assertIn(dict(op='combat_damage'), haste)

    def test_held_on_battlefield_until_real_end_and_then_owner_hand(self):
        name = "Alesha's Vanguard"
        for who in (0, 1):
            steps = self.case(name, f'dash_exact_{who}')['steps']
            end = next(i for i, s in enumerate(steps) if s['op'] == 'cleanup')
            self.assertEqual(self.count(steps[:end], who, 'battlefield', name), 1)
            self.assertEqual(self.count(steps, who, 'battlefield', name), 0)
            self.assertEqual(self.count(steps, who, 'hand', name), 1)
            self.assertEqual(self.count(steps, 1-who, 'hand', name), 0)
            self.assertEqual(self.count(steps, who, 'graveyard', name), 0)

    def test_real_control_transfer_returns_to_original_owner(self):
        name = 'Screamreach Brawler'
        for who in (0, 1):
            steps = self.case(name, f'donated_{who}')['steps']
            end = next(i for i, s in enumerate(steps) if s['op'] == 'cleanup')
            self.assertIn(dict(op='activate', ref='support', value=1), steps[:end])
            self.assertIn(dict(op='bind_permanent', ref='source', player=1-who, value=15009), steps[:end])
            self.assertEqual(self.count(steps[:end], 1-who, 'battlefield', name), 1)
            self.assertEqual(self.count(steps, who, 'hand', name), 1)
            self.assertEqual(self.count(steps, 1-who, 'hand', name), 0)

    def test_countered_or_destroyed_dash_stays_in_graveyard(self):
        name = 'Mardu Scout'
        for who in (0, 1):
            for mode in ('countered_dash', 'destroyed'):
                steps = self.case(name, f'{mode}_{who}')['steps']
                self.assertTrue(any(s['op'] == ('resolve_response' if mode == 'countered_dash' else 'destroy') for s in steps))
                self.assertEqual(self.count(steps, who, 'graveyard', name), 1)
                self.assertEqual(self.count(steps, who, 'hand', name), 0)

    def test_blinked_new_object_has_no_dash_haste_or_delayed_return(self):
        name = 'Kolaghan Skirmisher'
        for who in (0, 1):
            steps = self.case(name, f'blink_{who}')['steps']
            self.assertIn(dict(op='bind_permanent', ref='source', player=who, value=14947), steps)
            self.assertIn(dict(op='attack', ref='source', value=0), steps)
            self.assertEqual(self.count(steps, who, 'graveyard', 'Cloudshift'), 1)
            self.assertEqual(self.count(steps, who, 'battlefield', name), 1)

    def test_same_card_can_choose_either_cost_on_a_later_real_turn(self):
        name = "Alesha's Vanguard"
        for who in (0, 1):
            for dash in (False, True):
                steps = self.case(name, f'recast_{"dash" if dash else "normal"}_{who}')['steps']
                rebound = next(i for i, s in enumerate(steps) if s['op'] == 'bind_hand')
                self.assertEqual(sum(s['op'] == 'advance_turn' for s in steps[:rebound]), 2)
                self.assertEqual(self.count(steps[:rebound], who, 'hand', name), 1)
                self.assertEqual(self.count(steps, who, 'hand', name), int(dash))
                self.assertEqual(self.count(steps, who, 'battlefield', name), int(not dash))
                self.assertEqual(self.pool(steps, 'assert_mana', who), model.ZERO)

    def test_pending_and_return_snapshots_replay_only_the_actual_suffix(self):
        for mode, stack in [('pending_replay', 1), ('return_replay', 0)]:
            steps = self.case('Screamreach Brawler', f'{mode}_0')['steps']
            a, b, c = [next(i for i, s in enumerate(steps) if s['op'] == op) for op in ('capture', 'record_state', 'restore')]
            self.assertEqual(self.last(steps[:a], 'assert_stack'), stack)
            self.assertEqual(steps[a+1:b], steps[c+1:-1])
            self.assertFalse(any(s['op'] == 'cast' for s in steps[a+1:b]))


if __name__ == '__main__':
    unittest.main()
