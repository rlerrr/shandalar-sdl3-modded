"""Guard fight choices, loyalty payment and the printed library and emblem effects."""
import json
import unittest

from generate_cases import registry
import fight_battlefield_activations_recipes as model


class FightBattlefieldActivationsTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        data = model.PROFILE.parent
        cls.oracle = json.loads((data / 'oracle.json').read_bytes())['cards']
        cls.rows = {row['name']: row for row in registry() if row['kind'] != 'internal'}
        cls.homes = {}
        for name, pin in model.profiles()['sources'].items():
            home, = (data / 'cases/cards').glob(f"*/{pin['registry']['csvid']:05d}-*.json")
            cls.homes[name] = json.loads(home.read_bytes())['recipes'][0]

    def steps(self, name, variant):
        declaration = self.homes[name]
        shape = model.spec(declaration, self.oracle[name])
        return model.build(name, self.rows[name]['csvid'], declaration, shape, variant, self.rows, self.oracle)['steps']

    def test_printed_records_are_whole(self):
        model.validate_witnesses(self.oracle, self.rows)
        for name, declaration in self.homes.items():
            shape = model.spec(declaration, self.oracle[name])
            self.assertEqual(declaration['variants'], model.variants(shape))
            with self.assertRaises(ValueError):
                model.spec(declaration, dict(self.oracle[name], oracle_text='Fight.'))

    def test_domri_pays_loyalty_before_target_loss(self):
        for seat in (0, 1):
            steps = self.steps('Domri Rade', f'target_leaves_{seat}')
            activate = steps.index(dict(op='activate', ref='source', value=1))
            reply = steps.index(dict(op='cast', ref='response', value=1))
            self.assertLess(activate, reply)
            self.assertIn(dict(op='assert_counter', ref='source', counter_kind='loyalty', value=1), steps[activate:reply])
            self.assertIn(dict(op='assert_card', ref='fighter', property='damage', value=0), steps[reply:])
            self.assertIn(dict(op='can_activate', ref='source', value=0), steps[reply:])

    def test_domri_filters_only_the_top_card_and_earns_emblem(self):
        for seat in (0, 1):
            steps = self.steps('Domri Rade', f'top_noncreature_{seat}')
            self.assertIn(dict(op='assert_offered_count', value=1), steps)
            self.assertIn(dict(op='assert_offered_legal_card', index=self.rows['Forest']['csvid'], value=0), steps)
            steps = self.steps('Domri Rade', f'emblem_{seat}')
            loyalties = {item['value'] for item in steps if item['op'] == 'assert_counter'}
            self.assertEqual(loyalties, {3, 4, 5, 6, 7})
            self.assertEqual(sum(item['op'] == 'advance_turn' for item in steps), 8)
            bear = steps.index(dict(op='cast', ref='new_bear', value=1))
            attack = steps.index(dict(op='attack', ref='new_bear', value=1))
            self.assertNotIn('advance_turn', [item['op'] for item in steps[bear:attack]])
            self.assertIn(dict(op='can_target_creature', ref='bounce_probe', other='new_bear', value=0), steps[bear:attack])
            self.assertIn(dict(op='assert_life', player=1-seat, value=18), steps[attack:])


if __name__ == '__main__':
    unittest.main()
