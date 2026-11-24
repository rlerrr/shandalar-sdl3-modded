"""Focused tests for the entry-counter Kicker grammar and payment boundaries."""

from copy import deepcopy
from unittest import TestCase

from kicker_entry_counters_recipes import EntryCounterCase, FAMILY, profile, shape, spec, validate_witnesses


class EntryCounterKickerTest(TestCase):
    def setUp(self):
        pins = profile()
        records = pins['sources'] | pins['witnesses']
        self.oracle = {name: pin['oracle'] for name, pin in records.items()}
        self.registry = {name: pin['registry'] for name, pin in records.items()}

    def board(self, name, mode, actor=0):
        source = self.oracle[name]
        board = EntryCounterCase(source, actor, mode, self.registry, self.oracle, shape(source))
        board.run()
        return board

    def test_whole_oracle_and_registry_pins(self):
        name = 'Arctic Merfolk'
        source = self.oracle[name]
        declaration = dict(family=FAMILY, version=2, oracle_text=source['oracle_text'],
                           params=shape(source), variants=['paid_0'])
        self.assertEqual(spec(declaration, source)['returned_card'], 'Grizzly Bears')
        with self.assertRaises(ValueError):
            spec(declaration, dict(source, power='2'))
        with self.assertRaises(ValueError):
            spec(dict(declaration, variants=['paid_0', 'paid_0']), source)
        validate_witnesses(self.oracle, self.registry)
        wrong = deepcopy(self.registry)
        wrong[name]['handler'] = 'card_dummy'
        with self.assertRaises(ValueError):
            validate_witnesses(self.oracle, wrong)

    def test_kavu_decline_checks_unused_mana_before_zero_pool_untap(self):
        steps = self.board('Kavu Titan', 'declined', actor=1).steps
        advance = next(i for i, step in enumerate(steps) if step['op'] == 'advance_turn')
        self.assertIn(dict(op='assert_mana', player=1, color=0, value=2), steps[:advance])
        self.assertIn(dict(op='assert_mana', player=1, color=3, value=1), steps[:advance])
        self.assertEqual(steps[advance-7:advance-1],
                         [dict(op='set_mana', player=1, color=i, value=0) for i in range(6)])

    def test_creature_return_and_life_payment_happen_while_spell_is_pending(self):
        for name in ('Arctic Merfolk', 'Phyrexian Scuta'):
            with self.subTest(name=name):
                board = self.board(name, 'countered')
                steps = board.steps
                index = next(i for i, step in enumerate(steps) if step['op'] == 'announce_spell')
                pending = steps[:index]
                self.assertIn(dict(op='assert_stack', value=1), pending)
                if name == 'Arctic Merfolk':
                    self.assertIn(dict(op='assert_zone_multiplicity', player=0, zone='hand',
                                       index=self.registry['Grizzly Bears']['csvid'], value=1), pending)
                else:
                    self.assertIn(dict(op='assert_life', player=0, value=17), pending)
                self.assertNotIn('resolve', [step['op'] for step in pending])
                self.assertNotIn('assert_counter', [step['op'] for step in steps])

    def test_kicker_short_stays_unkicked_without_a_scripted_prompt(self):
        for name in ('Arctic Merfolk', 'Phyrexian Scuta', 'Kavu Titan'):
            with self.subTest(name=name):
                board = self.board(name, 'kicker_short')
                self.assertEqual(board.entry_count, 0)
                self.assertFalse(any(step['op'] == 'choose_dialog' for step in board.steps))
                self.assertIn(dict(op='cast', ref='source', value=1), board.steps)
                if name == 'Arctic Merfolk':
                    self.assertFalse(any(item['ref'] == 'return_cost' for item in board.setup))
                if name == 'Phyrexian Scuta':
                    self.assertIn(dict(op='assert_life', player=0, value=2), board.steps)
