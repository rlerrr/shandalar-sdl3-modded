"""Focused exact contracts for the damage/upkeep draw-step family."""
from copy import deepcopy
import unittest

from draw_step_damage_and_upkeep_recipes import (
    ASP, CLOCK, FAMILY, MASTICORE, VAULT, Board, classify, profiles, spec, validate_witnesses, variants,
)


def context():
    profile = profiles()
    oracle = {name: row['oracle'] for group in profile.values() for name, row in group.items()}
    rows = {name: row['registry'] for group in profile.values() for name, row in group.items()}
    return oracle, rows


def case(csvid, owner, variant):
    oracle, rows = context()
    game = Board(csvid, owner, variant, rows, oracle)
    game.build()
    return game


class DrawDamageAndUpkeep(unittest.TestCase):
    def test_whole_source_and_typed_recipe(self):
        oracle, _ = context()
        source = oracle['Nafs Asp']
        params = classify(source, ASP)
        declaration = dict(family=FAMILY, version=2, params=params, oracle_text=source['oracle_text'],
                           variants=variants(params))
        self.assertEqual(spec(declaration, source)['csvid'], ASP)
        for bad in (dict(declaration, version=2.0), dict(declaration, family='wrong'),
                    dict(declaration, variants=['paid_0', 'paid_0'])):
            with self.assertRaises(ValueError):
                spec(bad, source)
        with self.assertRaises(ValueError):
            spec(declaration, dict(source, oracle_text='Draw a card.'))

    def test_registered_witness_pins(self):
        oracle, rows = context()
        validate_witnesses(oracle, rows)
        bad = deepcopy(rows)
        bad['Personal Sanctuary']['iid'] += 1
        with self.assertRaises(ValueError):
            validate_witnesses(oracle, bad)

    def test_same_seat_draw_provenance_and_replay(self):
        for owner in (0, 1):
            steps = case(CLOCK, owner, 'replay').steps
            self.assertEqual([step['player'] for step in steps if step['op'] == 'draw_step'],
                             [1-owner, owner, owner, 1-owner, owner])
            for index, step in enumerate(steps):
                if step['op'] == 'draw_step':
                    predecessor = next(s for s in reversed(steps[:index]) if s['op'] in ('restore', 'upkeep_step'))
                    if predecessor['op'] == 'upkeep_step':
                        self.assertEqual(predecessor['player'], step['player'])
            self.assertEqual(sum(s['op'] == 'assert_recorded_state' for s in steps), 1)

    def test_clock_paid_opposing_upkeep_and_later_draw(self):
        board = case(CLOCK, 0, 'upkeep_payment')
        actions = board.steps
        active = next(index for index, item in enumerate(actions) if item['op'] == 'activate')
        prior = [s for s in actions[:active] if s['op'] == 'upkeep_step']
        self.assertEqual([s['player'] for s in prior], [1, 0, 1])
        self.assertTrue(any(s['op'] == 'cast' and s.get('ref') == 'clock_damage' for s in actions[:active]))
        before_pool = [(s['player'], s['value']) for s in actions[:active]
                       if s['op'] == 'set_mana' and s['color'] == 0]
        self.assertEqual(before_pool[-2:], [(0, 5), (1, 6)])
        residue = [(s['player'], s['value']) for s in actions[active:]
                   if s['op'] == 'assert_mana' and s['color'] == 0]
        self.assertEqual(residue[:2], [(0, 1), (1, 6)])
        self.assertEqual([s['op'] for s in actions[active:] if s['op'] in ('upkeep_step', 'draw_step')],
                         ['upkeep_step', 'draw_step'])
        self.assertEqual(board.life, [18, 16])

    def test_vault_produces_three_real_colorless(self):
        steps = case(VAULT, 1, 'tapped').steps
        paid = next(index for index, s in enumerate(steps) if s['op'] == 'cast' and s.get('ref') == 'mana_sink')
        self.assertTrue(any(s['op'] == 'assert_mana' and s['player'] == 1
                            and s['color'] == 0 and s['value'] == 3 for s in steps[:paid]))
        self.assertFalse(any(s['op'] == 'set_mana' and s['value'] == 3 for s in steps))

    def test_asp_life_loss_differs_from_preventable_combat_damage(self):
        for owner in (0, 1):
            self.assertEqual(case(ASP, owner, 'life_loss').life[1-owner], 18)
            self.assertEqual(case(ASP, owner, 'paid').life[1-owner], 19)
            fog = case(ASP, owner, 'fog')
            self.assertEqual(fog.life[1-owner], 20)
            self.assertTrue(any(s['op'] == 'cast' and s.get('ref') == 'fog' for s in fog.steps))

    def test_masticore_real_cost_targets_and_first_strike(self):
        for owner in (0, 1):
            game = case(MASTICORE, owner, 'first_strike')
            self.assertEqual(game.cards['discard_card']['zone'], 'graveyard')
            self.assertEqual(game.cards['blocker']['zone'], 'graveyard')
            self.assertEqual([(s['op'], s.get('ref'), s['player']) for s in game.steps if s['op'] == 'choose_target'],
                             [('choose_target', 'shot_target', owner)])
            steps = [s['op'] for s in game.steps]
            self.assertGreater(steps.index('combat_damage'), steps.index('advance_combat'))

