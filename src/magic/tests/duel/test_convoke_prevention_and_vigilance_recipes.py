"""Check whole Convoke records, typed branches and independent effect observations."""
from copy import deepcopy
import unittest

from convoke_prevention_and_vigilance_recipes import (
    ANGEL, CENTAUR, CHANT, FAMILY, GUARDIAN, Board, classify, profiles, spec, validate_witnesses, variants,
)


def inputs():
    pins = profiles()
    oracle = {name: pin['oracle'] for entries in pins.values() for name, pin in entries.items()}
    rows = {name: pin['registry'] for entries in pins.values() for name, pin in entries.items()}
    return oracle, rows


def scene(csvid, owner, mode):
    oracle, rows = inputs()
    name, = [name for name in profiles()['sources'] if rows[name]['csvid'] == csvid]
    board = Board(rows[name], owner, mode, rows, oracle)
    board.run()
    return board


class ConvokePreventionGuards(unittest.TestCase):
    def test_complete_oracle_and_typed_variant_contract(self):
        oracle, _ = inputs()
        record = oracle['Angel of Salvation']
        shape = classify(record, ANGEL)
        declaration = dict(family=FAMILY, version=2, params=shape, oracle_text=record['oracle_text'],
                           variants=variants(shape))
        self.assertEqual(spec(declaration, record)['csvid'], ANGEL)
        for invalid in (dict(declaration, version=2.0), dict(declaration, variants=['vigilance_0']),
                        dict(declaration, params=dict(shape, csvid=float(ANGEL)))):
            with self.assertRaises(ValueError):
                spec(invalid, record)
        with self.assertRaises(ValueError):
            spec(declaration, dict(record, oracle_text=record['oracle_text'].replace('next 5 damage', 'next 4 damage')))

    def test_whole_witnesses_and_registry_are_exact(self):
        oracle, rows = inputs()
        validate_witnesses(oracle, rows)
        altered = deepcopy(rows)
        altered['Squire']['iid'] += 1
        with self.assertRaises(ValueError):
            validate_witnesses(oracle, altered)

    def test_paid_convoke_distinguishes_chooser_and_tapped_cards(self):
        for owner in (0, 1):
            mixed = scene(GUARDIAN, owner, 'mixed')
            targets = [step['ref'] for step in mixed.steps if step['op'] == 'choose_target']
            self.assertEqual(targets, mixed.contributors if owner == 0 else [])
            self.assertTrue(all(mixed.cards[ref]['tapped'] for ref in mixed.contributors))
            self.assertFalse(mixed.cards['foreign']['tapped'])
            short = scene(CENTAUR, owner, 'short')
            self.assertEqual(short.cards['source']['zone'], 'hand')
            self.assertTrue(all(short.cards[ref]['tapped'] for ref in short.contributors))
            self.assertTrue(all(row['player'] in (0, 1) for row in short.setup))
            self.assertTrue(any(step['op'] == 'cast' and step['value'] == 0 for step in short.steps))

    def test_angel_real_parent_and_creature_split(self):
        for owner in (0, 1):
            game = scene(ANGEL, owner, 'creature')
            actions = [step['op'] for step in game.steps]
            self.assertLess(actions.index('announce_spell'), actions.index('cast_response'))
            self.assertLess(actions.index('cast_response'), actions.index('resolve_response'))
            self.assertEqual(game.cards['helper_0']['damage'], 1)
            self.assertEqual(game.cards['helper_0']['zone'], 'battlefield')
            self.assertEqual(game.life, [20, 20])

    def test_angel_five_points_are_consumed_across_two_packets(self):
        for owner in (0, 1):
            game = scene(ANGEL, owner, 'depletion')
            cast = [step['ref'] for step in game.steps if step['op'] in ('cast', 'cast_response')]
            self.assertEqual(cast, ['damage_parent', 'source', 'later_bolt'])
            self.assertEqual(game.life[owner], 19)
            self.assertEqual(game.life[1-owner], 20)
            self.assertEqual(game.shields[owner], 0)

    def test_chant_prevents_both_creature_combat_packets_and_gains_six(self):
        for owner in (0, 1):
            game = scene(CHANT, owner, 'creature_packets')
            block, = [step for step in game.steps if step['op'] == 'block']
            self.assertEqual((block['ref'], block['other'], block['value']), ('blocker', 'raider', 1))
            self.assertEqual(game.life[owner], 26)
            self.assertEqual(game.life[1-owner], 20)
            self.assertEqual([game.cards[ref]['zone'] for ref in ('raider', 'blocker')], ['battlefield'] * 2)
            self.assertEqual([game.cards[ref]['damage'] for ref in ('raider', 'blocker')], [0, 0])
            self.assertEqual(scene(CHANT, owner, 'expiry').life[owner], 19)

    def test_vigilance_uses_fresh_payment_helper_and_real_aged_attack(self):
        for source in (GUARDIAN, CENTAUR):
            for owner in (0, 1):
                game = scene(source, owner, 'vigilance')
                forms = [step['ref'] for step in game.steps if step['op'] == 'cast']
                self.assertEqual(forms, ['helper_0', 'source'])
                self.assertEqual(sum(step['op'] == 'advance_turn' for step in game.steps), 2)
                self.assertTrue(any(step['op'] == 'attack' and step['ref'] == 'source' and step['value'] == 1
                                    for step in game.steps))
                self.assertEqual(game.life[1-owner], 16)
                self.assertFalse(game.cards['source']['tapped'])
