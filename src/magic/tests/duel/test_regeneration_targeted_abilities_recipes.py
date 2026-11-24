"""Focused guards for typed, tap-cost, and hybrid regeneration actions."""

from copy import deepcopy
import unittest

import regeneration_targeted_abilities_recipes as model


class TargetedRegenerationTests(unittest.TestCase):
    def setUp(self):
        pins = model.profiles()
        all_pins = pins['sources'] | pins['witnesses']
        self.oracle = {name: entry['oracle'] for name, entry in all_pins.items()}
        self.rows = {name: entry['registry'] for name, entry in all_pins.items()}

    def case(self, name, mode, seat):
        game = model.FollowOn(self.rows[name]['csvid'], self.oracle[name], seat, mode, self.rows, self.oracle)
        game.build()
        return game

    def test_whole_oracle_and_declaration_are_exact(self):
        model.validate_witnesses(self.oracle, self.rows)
        oracle = deepcopy(self.oracle)
        oracle['Clockwork Gnomes']['oracle_text'] = oracle['Clockwork Gnomes']['oracle_text'].replace('artifact ', '')
        with self.assertRaises(ValueError):
            model.validate_witnesses(oracle, self.rows)
        card = self.oracle['Niall Silvain']
        shape = model.classify(card, model.NIALL)
        declaration = dict(family=model.FAMILY, version=2, oracle_text=card['oracle_text'], params=shape, variants=['own_0'])
        self.assertEqual(model.spec(declaration, card)['tap'], True)
        with self.assertRaises(ValueError):
            model.spec(declaration | {'version': 2.0}, card)

    def test_tap_cost_waits_and_already_tapped_requires_real_attack(self):
        for seat in (0, 1):
            active = self.case('Ragnar', 'proactive', seat)
            ops = [(step['op'], step.get('ref')) for step in active.steps]
            self.assertEqual(ops.count(('untap_step', None)), 2)
            self.assertLess(ops.index(('activate', 'source')), ops.index(('cast', 'removal')))
            self.assertNotIn(('choose_regeneration', 'source'), ops)
            tapped = self.case('Ragnar', 'tapped', seat)
            self.assertIn(dict(op='attack', ref='source', value=1), tapped.steps)
            self.assertEqual(tapped.cards['victim']['zone'], 'graveyard')
            self.assertEqual(tapped.life[1-seat], 18)

    def test_suture_hybrid_amounts_and_real_mixed_menu_selection(self):
        black, white = model.COLORS['B'], model.COLORS['W']
        for mode, split in (('white', (0, 3)), ('black', (3, 0)), ('mixed', (1, 2))):
            for seat in (0, 1):
                game = self.case('Suture Spirit', mode, seat)
                self.assertEqual((game.fee[black], game.fee[white]), split)
                self.assertFalse(any(step['op'] == 'choose_payment' for step in game.steps))
        game = self.case('Suture Spirit', 'hybrid_menu', 0)
        plan = [step['op'] for step in game.steps]
        self.assertLess(plan.index('choose_regeneration'), plan.index('choose_payment'))
        self.assertIn(dict(op='choose_payment', player=0, value=(1 << (4*black)) + (2 << (4*white))), game.steps)

    def test_gnome_type_boundaries_omit_illegal_target_input(self):
        allowed = self.case('Clockwork Gnomes', 'foreign', 1)
        self.assertEqual(allowed.cards['victim']['player'], 0)
        self.assertEqual(allowed.cards['victim']['zone'], 'battlefield')
        for mode, name in (('wrong_type', model.HOST), ('not_creature', model.ARTIFACT)):
            denied = self.case('Clockwork Gnomes', mode, 1)
            self.assertEqual(denied.cards['victim']['name'], name)
            self.assertEqual(denied.cards['victim']['zone'], 'graveyard')
            choices = [step for step in denied.steps if step['op'] == 'choose_target']
            self.assertEqual(len(choices), 1)
