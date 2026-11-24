"""Focused grammar and ordering checks for generated card-flow Kicker cases."""

from copy import deepcopy
from unittest import TestCase

from kicker_card_flow_recipes import CardFlowCase, FAMILY, profile, shape, spec, validate_witnesses


class CardFlowKickerTest(TestCase):
    def setUp(self):
        pins = profile()
        entries = pins['sources'] | pins['witnesses']
        self.oracle = {name: pin['oracle'] for name, pin in entries.items()}
        self.registry = {name: pin['registry'] for name, pin in entries.items()}

    def board(self, name, mode, actor=0):
        source = self.oracle[name]
        board = CardFlowCase(source, actor, mode, self.registry, self.oracle, shape(source))
        board.run()
        return board

    def test_exact_oracle_and_registry_pins(self):
        source = self.oracle['Jilt']
        declaration = dict(family=FAMILY, version=2, oracle_text=source['oracle_text'],
                           params=shape(source), variants=['paid_0'])
        self.assertEqual(spec(declaration, source)['effect'], 'bounce_damage')
        with self.assertRaises(ValueError):
            spec(declaration, dict(source, mana_cost='{U}'))
        with self.assertRaises(ValueError):
            spec(dict(declaration, variants=['search_declined_0']), source)
        validate_witnesses(self.oracle, self.registry)
        changed = deepcopy(self.registry)
        changed['Jilt']['handler'] = 'card_dummy'
        with self.assertRaises(ValueError):
            validate_witnesses(self.oracle, changed)

    def test_real_jilt_choice_order_and_target_invalidation(self):
        steps = self.board('Jilt', 'damage_target_lost', actor=1).steps
        first_cast = next(i for i, step in enumerate(steps) if step['op'] == 'cast')
        self.assertEqual(steps[first_cast-3:first_cast], [
            dict(op='choose_target', ref='return_target', player=1),
            dict(op='choose_dialog', player=1, value=0),
            dict(op='choose_target', ref='damage_target', player=1),
        ])
        after_cast = steps[first_cast+1:]
        self.assertIn(dict(op='cast_response', ref='reply', value=1), after_cast)
        for owner, name in ((0, 'Grizzly Bears'), (1, 'Hill Giant')):
            self.assertIn(dict(op='assert_zone_multiplicity', player=owner, zone='hand',
                               index=self.registry[name]['csvid'], value=1), after_cast)

    def test_kicker_short_keeps_paid_base_and_does_not_queue_extra(self):
        for name, expected in [('Jilt', [0, 0, 0, 0, 1, 0]), ('Gigantiform', [3, 0, 0, 0, 0, 0])]:
            with self.subTest(name=name):
                board = self.board(name, 'kicker_short')
                self.assertEqual(board.remaining, expected)
                self.assertIn(dict(op='cast', ref='source', value=1), board.steps)
                self.assertFalse(any(step['op'] == 'choose_dialog' for step in board.steps))

    def test_reveal_boundary_has_real_display_choices(self):
        steps = self.board('Thicket Elemental', 'no_creature', actor=1).steps
        self.assertEqual([step for step in steps if step['op'] == 'choose_zone'], [
            dict(op='choose_zone', value=-1, player=1),
            dict(op='choose_zone', value=-1, player=0),
        ])
        self.assertIn(dict(op='assert_zone', player=1, zone='library', value=3), steps)
        declined = self.board('Thicket Elemental', 'decline_trigger').steps
        self.assertNotIn('choose_zone', [s['op'] for s in declined])
