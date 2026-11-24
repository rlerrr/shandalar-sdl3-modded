"""Check whole Oracle, target scopes and paid execution shape."""
from copy import deepcopy
import unittest

from fight_two_target_and_granted_recipes import (
    DISSENSION, FAMILY, FEED, FEUD, TACTICS, Board, classify, profiles, spec, validate_witnesses, variants,
)


def inputs():
    pinned = profiles()
    oracle = {name: value['oracle'] for group in pinned.values() for name, value in group.items()}
    rows = {name: value['registry'] for group in pinned.values() for name, value in group.items()}
    return oracle, rows


def scene(csvid, owner, mode):
    oracle, rows = inputs()
    name = next(name for name in profiles()['sources'] if rows[name]['csvid'] == csvid)
    board = Board(oracle[name], owner, mode, rows, oracle)
    board.build()
    return board


class TwoTargetFightGuards(unittest.TestCase):
    def test_whole_oracle_keeps_target_order_and_death_rider(self):
        oracle, _ = inputs()
        record = oracle['Time to Feed']
        shape = classify(record, FEED)
        declaration = dict(family=FAMILY, version=2, params=shape, oracle_text=record['oracle_text'],
                           variants=variants(shape))
        self.assertEqual(spec(declaration, record)['csvid'], FEED)
        with self.assertRaises(ValueError):
            spec(declaration, dict(record, oracle_text=record['oracle_text'].replace('3 life', '2 life')))
        for bad in (dict(declaration, version=2.0), dict(declaration, family='other'),
                    dict(declaration, variants=['kill_now_0', 'kill_now_0'])):
            with self.assertRaises(ValueError):
                spec(bad, record)

    def test_exact_witness_registry(self):
        oracle, rows = inputs()
        validate_witnesses(oracle, rows)
        modified = deepcopy(rows)
        modified['Unsummon']['iid'] += 1
        with self.assertRaises(ValueError):
            validate_witnesses(oracle, modified)

    def test_blood_feud_same_opponent_independently_selects_each(self):
        for owner in (0, 1):
            game = scene(FEUD, owner, 'same_foe')
            self.assertEqual([s['ref'] for s in game.steps if s['op'] == 'choose_target'], ['first', 'second'])
            self.assertEqual([game.cards[ref]['player'] for ref in ('first', 'second')], [1-owner] * 2)
            self.assertEqual(game.cards['first']['zone'], 'graveyard')
            self.assertEqual(game.cards['second']['damage'], 1)

    def test_missing_fight_target_otherwise_ends_exchange(self):
        for owner in (0, 1):
            for lost, survivor in (('first', 'second'), ('second', 'first')):
                game = scene(FEUD, owner, f'lose_{lost}')
                self.assertEqual(game.cards[lost]['zone'], 'hand')
                self.assertEqual(game.cards[survivor]['damage'], 0)
                self.assertTrue(any(s['op'] == 'cast_response' for s in game.steps))

    def test_time_to_feed_own_target_loss_preserves_opposing_death_rider(self):
        for owner in (0, 1):
            late = scene(FEED, owner, 'lose_own')
            self.assertEqual([s['ref'] for s in late.steps if s['op'] == 'choose_target'],
                             ['second', 'first', 'first', 'second'])
            self.assertEqual(late.life[owner], 23)
            self.assertEqual(scene(FEED, owner, 'expired').life[owner], 20)

    def test_tactics_real_green_target_fee_and_expiration(self):
        for owner in (0, 1):
            game = scene(TACTICS, owner, 'two')
            self.assertEqual([s['ref'] for s in game.steps if s['op'] == 'choose_target'], ['first', 'second'])
            self.assertTrue(any(s['op'] == 'set_mana' and s['player'] == owner and s['color'] == 3 and s['value'] == 2
                                for s in game.steps))
            self.assertEqual([game.cards[ref]['boost'] for ref in ('first', 'second')], [0, 0])
            zero = scene(TACTICS, owner, 'zero')
            self.assertEqual([s['player'] for s in zero.steps if s['op'] == 'choose_done_target'], [owner])

    def test_dissension_targets_only_real_blockers(self):
        for owner in (0, 1):
            board = scene(DISSENSION, owner, 'blocks')
            block = [s['ref'] for s in board.steps if s['op'] == 'block']
            self.assertEqual(block, [s['ref'] for s in board.steps if s['op'] == 'choose_target'])
            actions = [s['op'] for s in board.steps]
            self.assertLess(actions.index('advance_combat'), actions.index('cast'))
            self.assertLess(actions.index('cast'), actions.index('combat_damage'))
            self.assertFalse(any(s['op'] == 'cast' for s in scene(DISSENSION, owner, 'one_blocker').steps))


    def test_each_paid_grant_uses_its_own_recipient_and_target(self):
        for owner in (0, 1):
            game = scene(TACTICS, owner, 'grant_two')
            choices = [(s['player'], s['ref']) for s in game.steps if s['op'] == 'choose_target']
            self.assertEqual(choices, [(owner, ref) for ref in ('first', 'second', 'spare', 'duelist_two')])
            activations = [s for s in game.steps if s['op'] == 'activate_granted_legacy']
            self.assertEqual([(s['ref'], s['index'], s['value']) for s in activations],
                             [('first', TACTICS, 1), ('second', TACTICS, 1)])
            self.assertTrue(any(s['op'] == 'set_mana' and s['player'] == owner
                                and s['color'] == 3 and s['value'] == 2 for s in game.steps))
            self.assertEqual([game.cards[ref]['zone'] for ref in ('spare', 'duelist_two')], ['graveyard'] * 2)
            aged = max(i for i, s in enumerate(game.steps) if s['op'] == 'untap_step')
            probes = [s for s in game.steps[aged:] if s['op'] == 'can_activate_granted_legacy']
            self.assertEqual([(s['ref'], s['value']) for s in probes], [('first', 0), ('second', 0)])

    def test_newly_cast_recipient_cannot_pay_tap_cost(self):
        for owner in (0, 1):
            game = scene(TACTICS, owner, 'grant_sick')
            casts = [s['ref'] for s in game.steps if s['op'] == 'cast']
            self.assertEqual(casts, ['first', 'source'])
            self.assertTrue(any(s['op'] == 'assert_card' and s.get('ref') == 'first'
                                and s.get('property') == 'sick' and s['value'] == 1 for s in game.steps))
            grants = [s for s in game.steps if s['op'] == 'activate_granted_legacy']
            self.assertEqual([(s['ref'], s['value']) for s in grants], [('first', 0), ('second', 1)])
            self.assertEqual(game.cards['duelist_two']['zone'], 'graveyard')

    def test_response_above_granted_activation_bounces_only_the_target(self):
        for owner in (0, 1):
            game = scene(TACTICS, owner, 'grant_target_lost')
            actions = [(i, s) for i, s in enumerate(game.steps) if s['op'] in ('activate_granted_legacy', 'cast')]
            (cast_source, source), (activate, ability), (cast_answer, answer) = actions
            self.assertLess(cast_source, activate)
            self.assertLess(activate, cast_answer)
            self.assertEqual([source['ref'], ability['ref'], answer['ref']], ['source', 'first', 'answer'])
            self.assertTrue(any(s['op'] == 'assert_stack' and s['value'] == 2 for s in game.steps[cast_answer:]))
            self.assertTrue(any(s['op'] == 'assert_card' and s.get('ref') == 'first'
                                and s.get('property') == 'tapped' and s['value'] == 1 for s in game.steps[cast_answer:]))
            self.assertEqual(game.cards['spare']['zone'], 'hand')
            self.assertEqual(game.cards['duelist_two']['zone'], 'battlefield')
