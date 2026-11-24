"""Guard pinned loyalty and paid Mill bodies without changing captured expectations."""
from copy import deepcopy
import unittest

from mill_remaining_activated_and_loyalty_recipes import (
    DEATH, ESTRID, FAMILY, HELM, HERETIC, HOPE, MAJESTY, Board, classify,
    profiles, spec, validate_witnesses, variants,
)


def inputs():
    pins = profiles()
    oracle = {name: pin['oracle'] for group in pins.values() for name, pin in group.items()}
    rows = {name: pin['registry'] for group in pins.values() for name, pin in group.items()}
    return oracle, rows


def scene(csvid, owner, mode):
    oracle, rows = inputs()
    name, = [name for name in profiles()['sources'] if rows[name]['csvid'] == csvid]
    game = Board(rows[name], owner, mode, rows, oracle)
    game.run()
    return game


class PaidMillGuards(unittest.TestCase):
    def test_whole_oracle_and_variant_receipt(self):
        oracle, _ = inputs()
        record = oracle['Helm of Obedience']
        shape = classify(record, HELM)
        declaration = dict(family=FAMILY, version=2, params=shape, oracle_text=record['oracle_text'],
                           variants=variants(shape))
        self.assertEqual(spec(declaration, record)['csvid'], HELM)
        for invalid in (dict(declaration, version=2.0), dict(declaration, variants=['walker_0']),
                        dict(declaration, params=dict(shape, csvid=float(HELM)))):
            with self.assertRaises(ValueError):
                spec(invalid, record)
        with self.assertRaises(ValueError):
            spec(declaration, dict(record, oracle_text=record['oracle_text'].replace("X can't be 0.", '')))

    def test_witnessed_oracle_and_token_data(self):
        oracle, rows = inputs()
        validate_witnesses(oracle, rows)
        altered = deepcopy(rows)
        altered['Mask']['iid'] += 1
        with self.assertRaises(ValueError):
            validate_witnesses(oracle, altered)

    def test_loyalty_model_keeps_full_oracle_despite_captured_defects(self):
        for seat in (0, 1):
            self.assertEqual(scene(ESTRID, seat, 'replenish').cards['source']['zone'], 'graveyard')
            drain = scene(DEATH, seat, 'drain')
            self.assertEqual((drain.life[seat], drain.life[1-seat]), (22, 18))
            returned = scene(MAJESTY, seat, 'reanimate')
            checks = [(s['property'], s['value']) for s in returned.steps if s['op'] == 'assert_matching_permanents']
            self.assertEqual(checks, [('color', 10), ('subtype_Zombie', 1), ('subtype_Bear', 1)])
            self.assertEqual(scene(HOPE, seat, 'emblem').zombies, 9)

    def test_helm_real_chosen_x_can_stop_early_and_transfers_controller(self):
        for seat in (0, 1):
            game = scene(HELM, seat, 'creature_second')
            choices = [(s['player'], s['value']) for s in game.steps if s['op'] == 'choose_x']
            self.assertEqual(choices, [(seat, 3)])
            self.assertEqual(len(game.libraries[1-seat]), 3)
            self.assertEqual(game.cards['source']['zone'], 'graveyard')
            stolen = game.cards[f'library_{1-seat}_1']
            self.assertEqual((stolen['name'], stolen['player'], stolen['zone']), ('Grizzly Bears', seat, 'battlefield'))
            self.assertEqual(game.pools[seat][0], 1)

    def test_helm_x_zero_and_unmet_creature_qualifier(self):
        for seat in (0, 1):
            zero = scene(HELM, seat, 'zero')
            self.assertEqual([s['value'] for s in zero.steps if s['op'] == 'activate'], [0])
            self.assertEqual(zero.pools[seat][0], 3)
            self.assertFalse(zero.cards['source']['tapped'])
            limit = scene(HELM, seat, 'limit')
            after = limit.cards[limit.libraries[1-seat][0]]
            self.assertEqual((after['name'], after['zone']), ('Grizzly Bears', 'library'))
            self.assertTrue(limit.cards['source']['tapped'])
            self.assertEqual(scene(HELM, seat, 'short_deck').libraries[1-seat], [])

    def test_heretic_uses_the_maximum_of_only_milled_cards(self):
        for seat in (0, 1):
            self.assertEqual(scene(HERETIC, seat, 'player').life[1-seat], 16)
            self.assertEqual(scene(HERETIC, seat, 'short_deck').life[1-seat], 18)
            self.assertEqual(scene(HERETIC, seat, 'land_only').life[1-seat], 20)
            damaged = scene(HERETIC, seat, 'creature')
            self.assertEqual(damaged.cards['victim']['zone'], 'graveyard')
            self.assertEqual(damaged.cards['control']['zone'], 'battlefield')
            self.assertEqual(scene(HERETIC, seat, 'walker').cards['victim']['zone'], 'graveyard')

    def test_heretic_targets_are_checked_before_milling(self):
        for seat in (0, 1):
            game = scene(HERETIC, seat, 'lost_target')
            self.assertEqual(game.cards['victim']['zone'], 'hand')
            self.assertEqual(len(game.libraries[seat]), 11)
            self.assertTrue(any(s['op'] == 'assert_stack' and s['value'] == 2 for s in game.steps))
            self.assertEqual(game.life, [20, 20])
