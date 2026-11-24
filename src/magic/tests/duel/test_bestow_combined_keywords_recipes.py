"""Guard whole source records and independent combat-life recipients."""
import json
import unittest

from generate_cases import registry
import bestow_combined_keywords_recipes as model


class BestowCombinedKeywordsTests(unittest.TestCase):
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

    def test_whole_source_and_auxiliary_identity_pins(self):
        model.validate_witnesses(self.oracle, self.rows)
        for name, declaration in self.homes.items():
            shape = model.spec(declaration, self.oracle[name])
            self.assertEqual(declaration['variants'], model.variants(shape))
            with self.assertRaises(ValueError):
                model.spec(declaration, dict(self.oracle[name], oracle_text='Lifelink'))
        tweaked = dict(self.rows)
        name = 'Hopeful Eidolon Bestowed'
        tweaked[name] = dict(tweaked[name], csvid=9999)
        with self.assertRaises(ValueError):
            model.validate_witnesses(self.oracle, tweaked)

    def test_chromanticore_assigns_each_damage_amount_once(self):
        for seat in (0, 1):
            for mode, attacker, damage, foe_damage in (('regular_combat', 'source', 4, 2),
                    ('bestowed_combat', 'host', 6, 4), ('detach_combat', 'source', 4, 2)):
                steps = self.steps('Chromanticore', f'{mode}_{seat}')
                self.assertIn(dict(op='assert_card', ref=attacker, property='tapped', value=0), steps)
                found = {item['player']: item['value'] for item in steps if item['op'] == 'assert_life'}
                self.assertEqual(found, {seat: 20+damage, 1-seat: 20-foe_damage})

    def test_hopeful_foreign_recipient_and_damage_on_death(self):
        for seat in (0, 1):
            steps = self.steps('Hopeful Eidolon', f'foe_link_{seat}')
            self.assertIn(dict(op='attack', ref='host', value=1), steps)
            self.assertIn(dict(op='assert_life', player=1-seat, value=23), steps)
            self.assertIn(dict(op='assert_life', player=seat, value=17), steps)
            steps = self.steps('Hopeful Eidolon', f'creature_link_{seat}')
            self.assertIn(dict(op='block', ref='blocker', other='source', value=1), steps)
            self.assertIn(dict(op='assert_card', ref='source', property='in_play', value=0), steps)
            self.assertIn(dict(op='assert_life', player=seat, value=21), steps)
            self.assertIn(dict(op='assert_life', player=1-seat, value=20), steps)


if __name__ == '__main__':
    unittest.main()
