"""Verify whole Oracle pins and the exact follower cost/recipient contracts."""
import json
import unittest

from generate_cases import registry
import mill_remaining_costs_and_grants_recipes as model


class MillRemainingCostsAndGrantsTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        data = model.PROFILE.parent
        cls.oracle = json.loads((data / 'oracle.json').read_bytes())['cards']
        cls.rows = {row['name']: row for row in registry() if row['kind'] != 'internal'}
        cls.homes = {}
        for name, pin in model.profiles()['sources'].items():
            home, = (data / 'cases/cards').glob(f"*/{pin['registry']['csvid']:05d}-*.json")
            cls.homes[name] = json.loads(home.read_bytes())['recipes'][0]

    def case(self, source, mode):
        declaration = self.homes[source]
        shape = model.spec(declaration, self.oracle[source])
        return model.expand(source, shape['csvid'], declaration, shape, [mode], self.rows, self.oracle)[0]

    def test_whole_records_and_witnesses(self):
        model.validate_witnesses(self.oracle, self.rows)
        for name, declaration in self.homes.items():
            shape = model.spec(declaration, self.oracle[name])
            self.assertEqual(declaration['variants'], model.variants(shape))
            with self.assertRaises(ValueError):
                model.spec(declaration, dict(self.oracle[name], oracle_text='Mill a card.'))
        self.assertEqual(self.oracle[model.FROST]['mana_cost'], '{1}{U}{U}')
        self.assertEqual(self.oracle[model.BLACK_DOUBLE]['mana_cost'], '{1}{B}{B}')
        self.assertEqual(self.oracle[model.TURTLE]['mana_cost'], '{2}{U}')

    def test_phenax_exact_devotion_and_toughness_at_resolution(self):
        name = 'Phenax, God of Deception'
        for seat in (0, 1):
            steps = self.case(name, f'devotion_threshold_{seat}')['steps']
            self.assertEqual([step['value'] for step in steps if step['op'] == 'can_target_creature'], [1, 1, 0])
            self.assertIn(dict(op='choose_target', player=seat, ref='host'), steps)
            steps = self.case(name, f'change_toughness_{seat}')['steps']
            self.assertIn(dict(op='assert_card', ref='host', property='toughness', value=7), steps)
            self.assertIn(dict(op='assert_zone', player=1-seat, zone='library', value=1), steps)
            cleanup = steps.index(dict(op='cleanup'))
            self.assertIn(dict(op='assert_card', ref='host', property='toughness', value=4), steps[cleanup:])

    def test_stern_mentor_taps_the_chosen_member_and_both_can_mill(self):
        for seat in (0, 1):
            steps = self.case('Stern Mentor', f'paired_both_{seat}')['steps']
            self.assertIn(dict(op='choose_dialog', player=seat, value=0), steps)
            self.assertEqual([action['value'] for action in steps if action['op'] == 'choose_player'], [1-seat, seat])
            for ref in ('source', 'partner'):
                self.assertIn(dict(op='assert_card', ref=ref, property='tapped', value=1), steps)
            declined = self.case('Stern Mentor', f'decline_{seat}')['steps']
            self.assertIn(dict(op='choose_optional_target', ref='partner', player=seat), declined)

    def test_paid_concoction_moves_costs_before_target_and_sliver_gap_is_explicit(self):
        for seat in (0, 1):
            steps = self.case('Sinister Concoction', f'destroy_{seat}')['steps']
            paid = steps.index(dict(op='activate', ref='source', value=1))
            done = steps.index(dict(op='resolve'), paid)
            self.assertIn(dict(op='assert_life', player=seat, value=19), steps[paid:done])
            self.assertIn(dict(op='assert_card', ref='victim', property='in_play', value=1), steps[paid:done])
            self.assertIn(dict(op='assert_card', ref='victim', property='in_play', value=0), steps[done:])
            sliver = self.case('Screeching Sliver', f'cast_{seat}')
            self.assertIn(dict(op='cast', ref='source', value=1), sliver['steps'])
            self.assertTrue(sliver['_recipe']['untested_components'])
            self.assertNotIn('activate', [s['op'] for s in sliver['steps']])


if __name__ == '__main__':
    unittest.main()
