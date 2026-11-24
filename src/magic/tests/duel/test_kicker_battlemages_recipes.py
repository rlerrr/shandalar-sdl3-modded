"""Focused guards for dual-kicker branch and payment independence."""

import unittest

import kicker_battlemages_recipes as model


class BattlemageGuard(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        pins = model.profiles()
        entries = pins['sources'] | pins['witnesses']
        cls.oracle = {name: pin['oracle'] for name, pin in entries.items()}
        cls.rows = {name: pin['registry'] for name, pin in entries.items()}
        cls.names = {row['csvid']: name for name, row in cls.rows.items()}

    def test_whole_declaration_includes_both_exact_costs_and_clauses(self):
        for csvid in model.EFFECTS:
            record = self.oracle[self.names[csvid]]
            shape = model.classify(record, csvid)
            declaration = dict(family=model.FAMILY, oracle_text=record['oracle_text'], params=shape,
                               version=2, variants=['first_0', 'second_0'])
            self.assertEqual(model.spec(declaration, record)['kicker'], shape['kicker'])
            wrong = dict(shape, kicker=list(reversed(shape['kicker'])))
            with self.assertRaises(ValueError):
                model.spec({**declaration, 'params': wrong}, record)
            with self.assertRaises(ValueError):
                model.classify({**record, 'oracle_text': record['oracle_text'] + '\nDraw a card.'}, csvid)

    def test_full_and_single_funded_rows_use_the_actual_menu_order(self):
        for csvid in model.EFFECTS:
            name = self.names[csvid]
            for seat in (0, 1):
                for mode, selected, row in (('base_only', (), 0), ('first_funded', (0,), 0),
                                            ('second_funded', (1,), 0), ('both', (0, 1), 2)):
                    game = model.Game(csvid, name, seat, mode, self.rows, self.oracle)
                    game.pay_and_cast()
                    dialogs = [s for s in game.steps if s['op'] == 'choose_dialog']
                    self.assertEqual([(s['player'], s['value']) for s in dialogs], [(seat, row)])
                    self.assertEqual(game.paid, selected)
                    self.assertEqual(game.pool_from_payment, [0] * 6)

    def test_nightscape_scope_stops_after_a_legal_first_target(self):
        nightscape = model.Game(5268, self.names[5268], 0, 'scope', self.rows, self.oracle)
        nightscape.build()
        selected = [s for s in nightscape.steps if s['op'] in ('choose_target', 'choose_cancel_target')]
        self.assertEqual([s['op'] for s in selected], ['choose_target', 'choose_cancel_target'])
        self.assertEqual(selected[0]['ref'], 'bounce1')
        self.assertEqual(nightscape.entities['bounce1']['zone'], 'hand')
        self.assertEqual(nightscape.entities['bounce2']['zone'], 'battlefield')
