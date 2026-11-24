"""Guard target ordering, payment, and pending combat and restriction replay."""
import json
import unittest

from generate_cases import registry
import kicker_combat_and_restrictions_recipes as model


class CombatRestrictionKickerTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        data = model.PROFILE.parent
        cls.oracle = json.loads((data / 'oracle.json').read_bytes())['cards']
        cls.rows = {row['name']: row for row in registry()}
        cls.homes = {}
        for name, pin in model.profiles()['sources'].items():
            home, = (data / 'cases/cards').glob(f"*/{pin['registry']['csvid']:05d}-*.json")
            cls.homes[name] = json.loads(home.read_bytes())['recipes'][0]

    def actions(self, name, mode):
        declaration = self.homes[name]
        shape = model.spec(declaration, self.oracle[name])
        return model.build(name, self.rows[name]['csvid'], declaration, shape, mode, self.rows, self.oracle)['steps']

    def test_complete_pins(self):
        model.validate_witnesses(self.oracle, self.rows)
        for name, declaration in self.homes.items():
            record = self.oracle[name]
            self.assertEqual(declaration['variants'], model.variants(model.spec(declaration, record)))
            with self.assertRaises(ValueError):
                model.spec(declaration, dict(record, mana_cost='{0}'))

    def test_target_and_additional_payment_order(self):
        for name, target in [("Orim's Chant", 'choose_player'), ("Vampire's Bite", 'choose_target'),
                             ('Vines of Vastwood', 'choose_target')]:
            paid = self.actions(name, 'paid_1')
            expected = [target, 'choose_dialog', 'cast']
            self.assertEqual([s['op'] for s in paid if s.get('ref') == 'source' or s['op'].startswith('choose')][:3], expected)
            for mode in ('base_only_0', 'wrong_kicker_color_1'):
                steps = self.actions(name, mode)
                self.assertFalse(any(s['op'] == 'choose_dialog' for s in steps))
                self.assertTrue(any(s['op'] == 'cast' and s.get('ref') == 'source' and s['value'] == 1 for s in steps))

    def test_responses_replay_and_untap(self):
        for name in self.homes:
            countered = self.actions(name, 'countered_paid_0')
            response = [s['op'] for s in countered if s['op'] in ('announce_spell', 'cast_response', 'resolve_response')]
            self.assertEqual(response, ['announce_spell', 'cast_response', 'resolve_response'])
            replay = self.actions(name, 'replay_1')
            ops = [s['op'] for s in replay]
            self.assertEqual(ops.count('choose_dialog'), 1)
            capture, record, restore, ai = map(ops.index, ('capture', 'record_state', 'restore', 'ai_roundtrip'))
            self.assertLess(capture, record)
            self.assertLess(record, restore)
            self.assertLess(restore, ai)
            self.assertEqual(replay[capture + 1:record], replay[ai + 1:-1])
            self.assertEqual(replay[-1]['op'], 'assert_recorded_state')
        expired = self.actions("Vampire's Bite", 'expired_combat_1')
        self.assertEqual(sum(s['op'] == 'combat_damage' for s in expired), 2)
        self.assertIn(dict(op='untap_step', player=1), expired)

    def test_printed_restrictions_and_legal_bystander(self):
        for seat in (0, 1):
            chant = self.actions("Orim's Chant", f'paid_{seat}')
            self.assertIn(dict(op='choose_player', player=seat, value=1-seat), chant)
            self.assertIn(dict(op='attack', ref='friend', value=0), chant)
            vines = self.actions('Vines of Vastwood', f'other_target_{seat}')
            self.assertIn(dict(op='can_target_creature', ref='opponent_probe', other='target', value=0), vines)
            self.assertIn(dict(op='can_target_creature', ref='opponent_probe', other='bystander', value=1), vines)


if __name__ == '__main__':
    unittest.main()
