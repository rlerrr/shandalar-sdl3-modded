"""Check paid mill source pins and independent new choice and payment receipts."""
import json
import unittest

from generate_cases import registry
from recipes import COLORS
import mill_paid_abilities_recipes as model


class MillPaidAbilitiesTests(unittest.TestCase):
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
        source = self.oracle[name]
        declaration = self.homes[name]
        shape = model.spec(declaration, source)
        return model.expand(name, shape['csvid'], declaration, shape, [mode], self.rows, self.oracle)[0]['steps']

    def test_whole_oracle_and_witness_pins(self):
        model.validate_witnesses(self.oracle, self.rows)
        for name, declaration in self.homes.items():
            shape = model.spec(declaration, self.oracle[name])
            self.assertEqual(declaration['variants'], model.variants(shape))
            with self.assertRaises(ValueError):
                model.spec(declaration, dict(self.oracle[name], oracle_text='{T}: Mill two cards.'))
        rows = dict(self.rows)
        rows[model.DESERT] = dict(rows[model.DESERT], csvid=9999)
        with self.assertRaises(ValueError):
            model.validate_witnesses(self.oracle, rows)

    def test_minister_cast_produces_two_energy_and_two_real_payments(self):
        for player in (0, 1):
            steps = self.steps('Minister of Inquiries', f'exhaust_{player}')
            self.assertIn(dict(op='cast', ref='source', value=1), steps)
            self.assertEqual([s['value'] for s in steps if s['op'] == 'activate'], [0, 1, 0, 1, 0, 0])
            self.assertEqual({s['value'] for s in steps if s['op'] == 'assert_energy' and s['player'] == player},
                             {0, 1, 2})
            self.assertEqual(sum(s['op'] == 'untap_step' and s['player'] == player for s in steps), 3)

    def test_timepiece_exact_choices_exile_and_unordered_shuffle(self):
        for player in (0, 1):
            steps = self.steps('Perpetual Timepiece', f'shuffle_one_{player}')
            receipt = steps.index(dict(op='assert_shuffle_count', player=player, value=1))
            self.assertTrue(any(s['op'] == 'choose_zone_card' for s in steps[:receipt]))
            self.assertIn(dict(op='assert_zone', player=player, zone='exile', value=1), steps[:receipt])
            self.assertFalse(any(s['op'] == 'assert_zone_card' and s['player'] == player and s['zone'] == 'library'
                                 for s in steps[receipt:]))
            empty = self.steps('Perpetual Timepiece', f'shuffle_zero_{player}')
            self.assertIn(dict(op='choose_zone', player=player, value=-1), empty)
            self.assertIn(dict(op='assert_shuffle_count', player=player, value=1), empty)

    def test_land_actions_keep_choices_and_exact_life_and_mana_cost(self):
        for player in (0, 1):
            stash = self.steps('Sequestered Stash', f'mill_new_artifact_{player}')
            self.assertIn(dict(op='play_land', ref='source', value=1), stash)
            self.assertIn(dict(op='choose_zone_card', player=player, value=self.rows[model.ARTIFACT]['csvid']), stash)
            self.assertIn(dict(op='assert_zone_card', player=player, zone='library', index=0,
                               value=self.rows[model.ARTIFACT]['csvid']), stash)
            rivulet = self.steps('Ipnu Rivulet', f'mill_self_{player}')
            paid = rivulet.index(dict(op='require_activation_mode', value=1))
            self.assertIn(dict(op='choose_player', player=player, value=player), rivulet[:paid])
            self.assertIn(dict(op='assert_mana', player=player, color=COLORS['U'], value=1), rivulet[paid:])
            blue = self.steps('Ipnu Rivulet', f'blue_{player}')
            free = self.steps('Ipnu Rivulet', f'colorless_{player}')
            self.assertIn(dict(op='assert_life', player=player, value=19), blue)
            self.assertNotIn(dict(op='assert_life', player=player, value=19), free)


if __name__ == '__main__':
    unittest.main()
