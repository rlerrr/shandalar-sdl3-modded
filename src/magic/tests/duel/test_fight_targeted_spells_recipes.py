"""Focused controls for paid fight timing and independent target validity."""

from copy import deepcopy
import unittest

from fight_targeted_spells_recipes import FAMILY, FightCase, classify, profiles, spec, variants


class FightTargetedSpellsTests(unittest.TestCase):
    def setUp(self):
        pins = profiles()['sources'] | profiles()['witnesses']
        self.rows = {name: pin['registry'] for name, pin in pins.items()}
        self.oracle = {name: pin['oracle'] for name, pin in pins.items()}

    def board(self, name, mode, owner):
        board = FightCase(self.oracle[name], self.rows[name]['csvid'], owner, mode, self.rows, self.oracle)
        board.play()
        return board

    def test_pounce_offturn_response_and_simultaneous_lethal_use_no_pump(self):
        for owner in (0, 1):
            offturn = self.board('Pounce', 'offturn_response', owner)
            self.assertEqual(offturn.steps[0], dict(op='turn', player=1 - owner))
            self.assertIn(dict(op='cast_response', ref='source', value=1), offturn.steps)
            self.assertEqual(offturn.effects[0]['paid_pump'], [0, 0])
            self.assertEqual(offturn.effects[0]['dealt_to_own_and_opposing'], [2, 2])
            self.assertEqual(offturn.cards['parent']['zone'], 'battlefield')
            deaths = self.board('Pounce', 'mutual_death', owner)
            self.assertEqual(deaths.effects[0]['dealt_to_own_and_opposing'], [1, 1])
            self.assertEqual([deaths.cards[ref]['zone'] for ref in ('own', 'foe')], ['graveyard'] * 2)

    def test_wild_instincts_keeps_its_legal_pump_after_opposing_target_loss(self):
        for mode, own_valid, opposing_valid, pump in (
                ('enemy_bounced', True, False, [2, 2]), ('own_bounced', False, True, [0, 0])):
            game = self.board('Wild Instincts', mode, 1)
            self.assertEqual(game.effects[0], dict(own_target_valid=own_valid,
                opposing_target_valid=opposing_valid, ferocious=False, paid_pump=pump,
                dealt_to_own_and_opposing=[0, 0]))
        countered = self.board('Wild Instincts', 'countered', 0)
        self.assertEqual(countered.effects, [])
        self.assertIn(dict(op='cast_response', ref='counter', value=1), countered.steps)
        self.assertEqual([countered.cards[ref]['zone'] for ref in ('source', 'counter')], ['graveyard'] * 2)

    def test_whole_oracle_checks_timing_and_paid_cost(self):
        record = self.oracle['Pounce']
        model = classify(record, 17480)
        declaration = dict(family=FAMILY, version=2, params=model, oracle_text=record['oracle_text'],
                           variants=variants(model))
        self.assertEqual(spec(declaration, record)['timing'], 'Instant')
        wrong = deepcopy(declaration)
        wrong['params']['mana'][0] += 1
        with self.assertRaises(ValueError):
            spec(wrong, record)
        with self.assertRaises(ValueError):
            spec(declaration, dict(record, type_line='Sorcery'))
