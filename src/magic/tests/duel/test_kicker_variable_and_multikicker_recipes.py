"""Guard complete Oracle classification, exact targets and real Kicker plans."""
import copy
import unittest

import kicker_variable_and_multikicker_recipes as model


class VariableKickerTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        pins = model.profile()
        cls.sources = pins['sources']
        all_pins = pins['sources'] | pins['witnesses']
        cls.oracle = {name: row['oracle'] for name, row in all_pins.items()}
        cls.rows = {name: row['registry'] for name, row in all_pins.items()}

    def actions(self, name, variant):
        record = self.oracle[name]
        declaration = dict(family=model.FAMILY, version=2, variants=[variant],
                           oracle_text=record['oracle_text'], params=model.parameters(record))
        shape = model.spec(declaration, record)
        case, = model.expand(name, self.rows[name]['csvid'], declaration, shape, [variant], self.rows, self.oracle)
        return case['steps']

    def test_full_printed_oracle_and_pins(self):
        model.validate_witnesses(self.oracle, self.rows)
        for name in self.sources:
            printed = copy.deepcopy(self.oracle[name])
            printed['oracle_text'] += '\nFlying'
            with self.subTest(name=name), self.assertRaises(ValueError):
                model.parameters(printed)

    def test_deathforge_sentinel_is_one_extra_legal_target(self):
        for actor in (0, 1):
            for mode in ('declined', 'paid'):
                actions = self.actions('Deathforge Shaman', f'{mode}_{actor}')
                settle = next(i for i, action in enumerate(actions) if action['op'] == 'resolve')
                self.assertEqual(actions[settle - 1], dict(op='choose_player', player=actor, value=1 - actor))
                self.assertEqual(actions[settle + 1], dict(op='assert_unused_choices', value=1))
                self.assertEqual(actions[settle - 2]['op'], 'choose_target' if mode == 'paid' else 'choose_player')
                self.assertTrue(all(a['value'] == 20 for a in actions if a['op'] == 'assert_life'))

    def test_comet_and_x_fee_selectors_stay_distinct(self):
        for actor in (0, 1):
            actions = self.actions('Comet Storm', f'once_{actor}')
            relevant = [a for a in actions if a['op'] in ('choose_dialog', 'choose_x', 'choose_target')]
            self.assertEqual(relevant, [dict(op='choose_x', player=actor, value=2),
                                        dict(op='choose_target', ref='ground', player=actor)])
            actions = self.actions('Kangee, Aerie Keeper', f'once_{actor}')
            relevant = [a for a in actions if a['op'] in ('choose_dialog', 'choose_x', 'choose_target')]
            self.assertEqual(relevant, [dict(op='choose_dialog', player=actor, value=0),
                                        dict(op='choose_x', player=actor, value=1)])

    def test_paid_kicker_can_be_countered_and_replayed(self):
        actions = self.actions("Marshal's Anthem", 'countered_0')
        trace = [a['op'] for a in actions if a['op'] in
                 ('cast', 'announce_spell', 'cast_response', 'resolve_response', 'choose_zone_card')]
        self.assertEqual(trace, ['cast', 'announce_spell', 'cast_response', 'resolve_response'])
        replay = self.actions('Verdeloth the Ancient', 'replay_1')
        self.assertEqual([a['value'] for a in replay if a['op'] == 'choose_x'], [2, 2])
        self.assertEqual([a['op'] for a in replay if a['op'] in ('capture', 'restore', 'assert_recorded_state')],
                         ['capture', 'restore', 'assert_recorded_state'])


if __name__ == '__main__':
    unittest.main()
