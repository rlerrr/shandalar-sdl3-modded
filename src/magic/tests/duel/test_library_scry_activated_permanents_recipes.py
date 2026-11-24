"""Check exact Oracle pins and distinct paid Scry riders."""
import json
import unittest

from generate_cases import registry
import library_scry_activated_permanents_recipes as model


class LibraryScryActivatedPermanentTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        data = model.PROFILE.parent
        cls.oracle = json.loads((data / 'oracle.json').read_bytes())['cards']
        cls.rows = {row['name']: row for row in registry() if row['kind'] != 'internal'}
        cls.homes = {}
        for name, pin in model.profiles()['sources'].items():
            home, = (data / 'cases/cards').glob(f"*/{pin['registry']['csvid']:05d}-*.json")
            cls.homes[name] = json.loads(home.read_bytes())['recipes'][0]

    def steps(self, name, mode):
        record = self.oracle[name]
        declaration = self.homes[name]
        shape = model.spec(declaration, record)
        return model.expand(name, shape['csvid'], declaration, shape, [mode], self.rows, self.oracle)[0]['steps']

    def test_whole_oracle_and_servo_pins(self):
        model.validate_witnesses(self.oracle, self.rows)
        for name, declaration in self.homes.items():
            with self.assertRaises(ValueError):
                model.spec(declaration, dict(self.oracle[name], oracle_text='Scry 1.'))
        self.assertEqual(self.oracle[model.SERVO]['type_line'], 'Token Artifact Creature — Servo')

    def test_prism_cast_color_count_and_aether_energy_pay_through_actual_actions(self):
        for seat in (0, 1):
            for mode, expected in (('one_crystal', 1), ('two_crystals', 2)):
                steps = self.steps('Prism Array', f'{mode}_{seat}')
                self.assertIn(dict(op='cast', ref='source', value=1), steps)
                self.assertEqual(sum(s['op'] == 'activate' and s['value'] == 1 for s in steps), expected)
                self.assertIn(dict(op='activate', ref='source', value=0), steps)
            steps = self.steps('Aether Theorist', f'exhaust_{seat}')
            self.assertEqual(sum(s['op'] == 'activate' and s['value'] == 1 for s in steps), 3)
            self.assertEqual({s['value'] for s in steps if s['op'] == 'assert_energy' and s['player'] == seat},
                             {0, 1, 2, 3})

    def test_thrasios_post_scry_destination_uses_typed_library_card(self):
        for seat in (0, 1):
            land = self.steps('Thrasios, Triton Hero', f'land_{seat}')
            self.assertIn(dict(op='bind_permanent_copy', ref='found', player=seat, index=0,
                               value=self.rows['Forest']['csvid']), land)
            self.assertIn(dict(op='assert_card', ref='found', property='tapped', value=1), land)
            draw = self.steps('Thrasios, Triton Hero', f'draw_{seat}')
            self.assertIn(dict(op='assert_zone_card', player=seat, zone='hand', index=0,
                               value=self.rows['Grizzly Bears']['csvid']), draw)

    def test_stockpile_revolt_and_bontu_pay_real_owned_sacrifices(self):
        servo = self.rows[model.SERVO]['csvid']
        for seat in (0, 1):
            stock = self.steps('Hidden Stockpile', f'revolt_sac_{seat}')
            boundary = stock.index(dict(op='advance_turn'))
            self.assertIn(dict(op='assert_zone_multiplicity', player=seat, zone='battlefield',
                               index=servo, value=0), stock[:boundary])
            self.assertIn(dict(op='assert_zone_multiplicity', player=seat, zone='battlefield',
                               index=servo, value=1), stock[boundary:])
            bontu = self.steps('Bontu the Glorified', f'menace_after_sac_{seat}')
            self.assertIn(dict(op='assert_life', player=seat, value=21), bontu)
            self.assertIn(dict(op='assert_life', player=1-seat, value=15), bontu)
            self.assertIn(dict(op='block', ref='blocker', other='source', value=0), bontu)


if __name__ == '__main__':
    unittest.main()
