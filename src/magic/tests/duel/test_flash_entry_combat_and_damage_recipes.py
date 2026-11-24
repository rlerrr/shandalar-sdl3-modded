"""Guard whole Flash sources and distinct response, damage and attack behaviors."""
from copy import deepcopy
import unittest

from flash_entry_combat_and_damage_recipes import (
    ANGEL, BARRIER, CLOUD, FAMILY, HELLKITE, SPIRIT, Board, classify, profiles, spec, validate_witnesses, variants,
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


class FlashEntryCombatGuards(unittest.TestCase):
    def test_whole_oracle_and_typed_variants(self):
        oracle, _ = inputs()
        record = oracle['Nivix Barrier']
        shape = classify(record, BARRIER)
        declaration = dict(family=FAMILY, version=2, params=shape, oracle_text=record['oracle_text'],
                           variants=variants(shape))
        self.assertEqual(spec(declaration, record)['csvid'], BARRIER)
        for invalid in (dict(declaration, version=2.0), dict(declaration, variants=['face_0']),
                        dict(declaration, params=dict(shape, csvid=float(BARRIER)))):
            with self.assertRaises(ValueError):
                spec(invalid, record)
        with self.assertRaises(ValueError):
            spec(declaration, dict(record, oracle_text=record['oracle_text'].replace('attacking ', '')))

    def test_whole_witness_and_source_registry(self):
        oracle, rows = inputs()
        validate_witnesses(oracle, rows)
        altered = deepcopy(rows)
        altered['Krosan Tusker']['iid'] += 1
        with self.assertRaises(ValueError):
            validate_witnesses(oracle, altered)

    def test_angel_hand_cast_and_real_paid_blink_differ(self):
        for seat in (0, 1):
            cast = scene(ANGEL, seat, 'enemy_attackers')
            blink = scene(ANGEL, seat, 'blink')
            self.assertEqual([cast.cards[ref]['zone'] for ref in ('bear', 'drake')], ['exile'] * 2)
            self.assertEqual([blink.cards[ref]['zone'] for ref in ('bear', 'drake')], ['battlefield'] * 2)
            self.assertEqual(blink.life[seat], 16)
            self.assertTrue(any(step['op'] == 'bind_permanent' for step in blink.steps))

    def test_hellkite_real_divided_damage_and_cloudthresher_evoke(self):
        for seat in (0, 1):
            dragon = scene(HELLKITE, seat, 'split_response')
            spell = scene(CLOUD, seat, 'evoke_response')
            self.assertEqual(dragon.life[1-seat], 17)
            self.assertEqual(dragon.life[seat], 19)
            self.assertEqual(dragon.cards['attacker']['zone'], 'graveyard')
            self.assertEqual(spell.cards['source']['zone'], 'graveyard')
            self.assertEqual(spell.cards['my_flyer']['zone'], 'graveyard')
            self.assertEqual(spell.cards['foe_ground']['damage'], 0)
            self.assertEqual(spell.life[seat], 14)

    def test_blocker_removal_does_not_unblock_attacker(self):
        for seat in (0, 1):
            game = scene(SPIRIT, seat, 'remove_blocker')
            self.assertEqual(game.life, [20, 20])
            self.assertEqual(game.cards['blocker']['zone'], 'battlefield')
            observations = [step['value'] for step in game.steps if step['op'] == 'assert_card'
                            and step.get('ref') == 'attacker' and step.get('property') == 'blocked']
            self.assertEqual(observations, [1, 1])

    def test_barrier_timing_target_scope_and_real_damage(self):
        for seat in (0, 1):
            game = scene(BARRIER, seat, 'response')
            actions = [step['op'] for step in game.steps]
            self.assertLess(actions.index('finish_attackers'), actions.index('cast_response'))
            self.assertLess(actions.index('announce_spell'), actions.index('cast_response'))
            choices = [(step['player'], step['ref']) for step in game.steps if step['op'] == 'choose_target']
            self.assertEqual(choices, [(1-seat, 'attacker'), (seat, 'attacker')])
            self.assertEqual(game.life[seat], 15)
            self.assertEqual(scene(BARRIER, seat, 'shrink').life[seat], 14)
            block = scene(BARRIER, seat, 'fresh_block')
            self.assertEqual(block.cards['source']['damage'], 2)
            self.assertEqual(block.life[seat], 20)

    def test_barrier_ages_and_defender_remains_binding(self):
        for seat in (0, 1):
            expiry = scene(BARRIER, seat, 'expiry')
            self.assertEqual(expiry.life[seat], 12)
            self.assertEqual(sum(step['op'] == 'advance_turn' for step in expiry.steps), 2)
            defender = scene(BARRIER, seat, 'defender')
            self.assertFalse(any(step['op'] == 'choose_target' for step in defender.steps))
            attacks = [(step['ref'], step['value']) for step in defender.steps if step['op'] == 'attack']
            self.assertEqual(attacks, [('source', 0), ('idle', 1)])
            self.assertEqual(defender.life[1-seat], 17)
