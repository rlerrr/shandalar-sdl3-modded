"""Focused guards for variable Suspend's real payment, phases and P/T effects."""
import copy
import unittest

import suspend_variable_countdown_recipes as model


class VariableSuspendTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        pins = model.profile()
        cls.sources = pins['sources']
        combined = pins['sources'] | pins['witnesses']
        cls.oracle = {name: row['oracle'] for name, row in combined.items()}
        cls.rows = {name: row['registry'] for name, row in combined.items()}

    def case(self, name, variant):
        card = self.oracle[name]
        declaration = dict(family=model.FAMILY, version=2, oracle_text=card['oracle_text'],
                           params=model.parameters(card), variants=[variant])
        shape = model.spec(declaration, card)
        case, = model.expand(name, self.rows[name]['csvid'], declaration, shape, [variant], self.rows, self.oracle)
        return case

    def test_whole_pins_and_positive_x_required(self):
        model.validate_witnesses(self.oracle, self.rows)
        for name in self.sources:
            bad = copy.deepcopy(self.oracle[name])
            bad['oracle_text'] = bad['oracle_text'].replace("X can't be 0.", 'X may be 0.')
            with self.subTest(name=name), self.assertRaises(ValueError):
                model.parameters(bad)

    def test_actual_x_matches_each_owner_upkeep_and_witness_kind(self):
        for name in self.sources:
            actions = self.case(name, 'x3_1')['steps']
            self.assertEqual([s['value'] for s in actions if s['op'] == 'choose_x'], [3])
            self.assertEqual([s['player'] for s in actions if s['op'] == 'upkeep_step'], [0, 1] * 3)
            last = max(i for i, s in enumerate(actions) if s['op'] == 'upkeep_step')
            self.assertIn(dict(op='attack', ref='played', value=1), actions[last:])
        detritivore = self.case('Detritivore', 'x3_0')['steps']
        self.assertEqual([s['ref'] for s in detritivore if s['op'] == 'choose_target'], ['land_0', 'land_1', 'land_2'])

    def test_fungal_true_modular_seed_and_aeon_real_hand_power(self):
        fungal = self.case('Fungal Behemoth', 'x1_0')
        [seed] = [c for c in fungal['setup'] if c['ref'] == 'existing_counter']
        self.assertEqual(seed['name'], 'Arcbound Worker')
        self.assertTrue(all(a['value'] == 1 for a in fungal['steps'] if a['op'] == 'assert_counter'
                            and a.get('ref') == 'existing_counter'))
        aeon = self.case('Aeon Chronicler', 'x2_0')['steps']
        self.assertIn(dict(op='assert_card', ref='played', property='power', value=3), aeon)

    def test_replay_initializes_x_once_and_wrong_color_never_selects_it(self):
        actions = self.case('Benalish Commander', 'replay_1')['steps']
        start = actions.index(dict(op='capture'))
        record = actions.index(dict(op='record_state'))
        again = actions.index(dict(op='ai_roundtrip'))
        self.assertEqual(actions[start + 1:record], actions[again + 1:-1])
        self.assertEqual([a['value'] for a in actions[:start] if a['op'] == 'choose_x'], [2])
        wrong = self.case('Roiling Horror', 'wrong_color_0')['steps']
        self.assertIn(dict(op='cast', ref='source', value=0), wrong)
        self.assertFalse(any(a['op'] in ('choose_x', 'upkeep_step') for a in wrong))


if __name__ == '__main__':
    unittest.main()
