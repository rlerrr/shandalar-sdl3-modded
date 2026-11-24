"""Guards for the whole-Oracle multikicker model and real payment plans."""

import copy
import unittest

import kicker_multikicker_counters_recipes as model


class MultikickerTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        data = model.profile()
        all_pins = data['sources'] | data['witnesses']
        cls.sources = data['sources']
        cls.oracle = {name: pin['oracle'] for name, pin in all_pins.items()}
        cls.rows = {name: pin['registry'] for name, pin in all_pins.items()}

    def materialize(self, name, variant):
        printed = self.oracle[name]
        declaration = dict(family=model.FAMILY, version=2, variants=[variant],
                           oracle_text=printed['oracle_text'], params=model.parameters(printed))
        shape = model.spec(declaration, printed)
        case, = model.expand(name, self.rows[name]['csvid'], declaration, shape, [variant], self.rows, self.oracle)
        return case['steps']

    def test_whole_records_and_strict_version(self):
        model.validate_witnesses(self.oracle, self.rows)
        for name in self.sources:
            source = self.oracle[name]
            changed = copy.deepcopy(source)
            changed['oracle_text'] += '\nFlying'
            with self.subTest(name=name), self.assertRaises(ValueError):
                model.parameters(changed)
        source = self.oracle['Gnarlid Pack']
        with self.assertRaisesRegex(ValueError, 'Exact complete'):
            model.spec(dict(family=model.FAMILY, version=2.0, oracle_text=source['oracle_text'],
                            params=model.parameters(source), variants=['twice_0']), source)

    def test_mana_plan_carries_exact_remaining_fee(self):
        for count, mode in enumerate(('declined', 'once', 'twice', 'thrice')):
            with self.subTest(mode=mode):
                actions = self.materialize('Quag Vampires', mode + '_1')
                self.assertEqual([s['value'] for s in actions if s['op'] == 'choose_dialog'], [0] * count + [1])
                casts = next(i for i, s in enumerate(actions) if s['op'] == 'cast')
                self.assertEqual([s['value'] for s in actions[:casts] if s['op'] == 'set_mana'
                                  and s['player'] == 1], [count + 1, count + 2, 0, 0, 0, 0])
                self.assertEqual([s['value'] for s in actions[casts:] if s['op'] == 'assert_mana'
                                  and s['player'] == 1], [1, 1, 0, 0, 0, 0] * 2)
                self.assertTrue(any(s['op'] == 'assert_counter' and s['value'] == count for s in actions))

    def test_countered_and_replay_resolve_actual_stack(self):
        counter = self.materialize('Enclave Elite', 'countered_0')
        sequence = [s['op'] for s in counter if s['op'] in
                    ('cast', 'announce_spell', 'cast_response', 'resolve_response', 'assert_counter')]
        self.assertEqual(sequence, ['cast', 'announce_spell', 'cast_response', 'resolve_response'])
        self.assertNotIn('choose_target', [s['op'] for s in counter])
        replay = self.materialize('Apex Hawks', 'replay_1')
        self.assertEqual([s['value'] for s in replay if s['op'] == 'choose_dialog'], [0, 0, 0, 1] * 2)
        self.assertEqual([s['op'] for s in replay if s['op'] in ('capture', 'restore', 'assert_recorded_state')],
                         ['capture', 'restore', 'assert_recorded_state'])

    def test_short_funding_stops_without_fake_choice(self):
        for mode, plan in [('exact_two', [0, 0]), ('short_second', [0]), ('wrong_kicker_color', []), ('short_base', [])]:
            actions = self.materialize('Gnarlid Pack', mode + '_0')
            self.assertEqual([s['value'] for s in actions if s['op'] == 'choose_dialog'], plan)
            self.assertEqual([s['value'] for s in actions if s['op'] == 'cast'], [int(mode != 'short_base')])


if __name__ == '__main__':
    unittest.main()
