"""Focused guards for the pinned Threshold modal payment and choice paths."""

from copy import deepcopy
from unittest import TestCase

from threshold_modal_activations_recipes import (
    FAMILY, ModalActivationCase, profile, shape, spec, validate_witnesses,
)


class ThresholdModalTest(TestCase):
    def setUp(self):
        pins = profile()
        self.oracle = {name: pin['oracle'] for name, pin in (pins['sources'] | pins['witnesses']).items()}
        self.registry = {name: pin['registry'] for name, pin in (pins['sources'] | pins['witnesses']).items()}

    def board(self, name, mode, owner):
        board = ModalActivationCase(self.oracle[name], self.registry[name]['csvid'],
                                    owner, mode, self.registry, self.oracle)
        board.assemble()
        return board

    def test_pinned_record_and_handler(self):
        source = self.oracle['Nantuko Monastery']
        declaration = dict(family=FAMILY, version=2, oracle_text=source['oracle_text'],
                           params=shape(source), variants=['short_fee_0'])
        self.assertEqual(spec(declaration, source)['source'], 5979)
        with self.assertRaises(ValueError):
            spec(declaration, dict(source, oracle_id='wrong'))
        with self.assertRaises(ValueError):
            spec(dict(declaration, variants=['zero_eligible_1']), source)
        wrong = deepcopy(self.registry)
        wrong[source['name']]['handler'] = 'card_dummy'
        with self.assertRaises(ValueError):
            validate_witnesses(self.oracle, wrong)

    def test_partial_bottoming_payment_cancels_without_moving_three_cards(self):
        board = self.board('Battlefield Scrounger', 'cancel_cost', 0)
        selected = [step for step in board.steps if step['op']=='choose_zone_card']
        self.assertEqual(len(selected), 2)
        self.assertIn(dict(op='choose_zone', player=0, value=-1), board.steps)
        self.assertIn(dict(op='activate', ref='source', value=0), board.steps)
        self.assertEqual((board.own_count, board.bottomed), (7, 0))

    def test_short_enhanced_fee_keeps_base_mode(self):
        for source in ('Infected Vermin', 'Nomad Decoy'):
            for owner in (0,1):
                with self.subTest(source=source, owner=owner):
                    board = self.board(source, 'short_fee', owner)
                    self.assertTrue(board.threshold)
                    self.assertEqual([s['value'] for s in board.steps if s['op']=='choose_dialog'], [0] if owner else [])
                    self.assertIn(dict(op='activate', ref='source', value=1), board.steps)

    def test_monastery_calls_real_mana_then_checks_short_paid_animation(self):
        board = self.board('Nantuko Monastery', 'short_fee', 0)
        self.assertEqual([s['value'] for s in board.steps if s['op']=='activate' and s['ref']=='source'], [1, 0])
        self.assertIn(dict(op='assert_mana', player=0, color=0, value=1), board.steps)
        self.assertIn(dict(op='assert_mana', player=0, color=3, value=1), board.steps)
        self.assertIn(dict(op='can_target_creature', ref='land_probe', other='source', value=0), board.steps)

    def test_restorer_one_named_land_and_two_safe_cancellations(self):
        board = self.board('Krosan Restorer', 'choose_one', 1)
        after_can_activate = next(i for i,s in enumerate(board.steps) if s['op']=='can_activate') + 1
        self.assertEqual(board.steps[after_can_activate:after_can_activate+3], [
            dict(op='choose_target', player=1, ref='land_0'),
            dict(op='choose_cancel_target', player=1),
            dict(op='choose_cancel_target', player=1),
        ])
