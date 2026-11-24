"""Guard audited coin phases, damage-window RNG and real negative controls."""
import json
import unittest

from generate_cases import registry
import coin_flip_combat_flips_recipes as model


class CombatCoinFlipsTests(unittest.TestCase):
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

    def test_sources_and_witnesses_reject_text_mutations(self):
        model.validate_witnesses(self.oracle, self.rows)
        for name, declaration in self.homes.items():
            shape = model.spec(declaration, self.oracle[name])
            self.assertEqual(declaration['variants'], model.variants(shape))
            with self.assertRaises(ValueError):
                model.spec(declaration, dict(self.oracle[name], oracle_text='Flip a coin.'))
        with self.assertRaises(ValueError):
            model.validate_witnesses(dict(self.oracle, Shock=dict(self.oracle['Shock'], mana_cost='{U}')), self.rows)

    def test_coin_input_uses_the_real_declaration_or_damage_window(self):
        for seat in (0, 1):
            for name, scene, phase, call in (
                ('Creepy Doll', 'win', 'combat_damage', 0),
                ('Mijae Djinn', 'win', 'finish_attackers', 1),
                ('Ydwen Efreet', 'win', 'advance_combat', 1),
                ('Volatile Rig', 'trample_win', 'combat_damage', 0),
            ):
                steps = self.steps(name, f'{scene}_{seat}')
                index, = [i for i, step in enumerate(steps) if step['op'] == 'choose_dialog']
                self.assertEqual(steps[index:index+2], [dict(op='choose_dialog', player=seat, value=call), dict(op=phase)])
            steps = self.steps('Volatile Rig', f'damage_sacrifice_{seat}')
            first, second = [i for i, step in enumerate(steps) if step['op'] == 'choose_dialog']
            self.assertEqual(steps[first:second+2], [dict(op='choose_dialog', player=seat, value=1),
                                                    dict(op='choose_dialog', player=seat, value=1), dict(op='resolve')])

    def test_distinct_destruction_exile_and_replay_controls(self):
        for seat in (0, 1):
            for name, mode, zone in (('Creepy Doll', 'destroy_source', 'battlefield'),
                                     ('Volatile Rig', 'exile_instead', 'exile')):
                steps = self.steps(name, f'{mode}_{seat}')
                self.assertFalse(any(step['op'] == 'choose_dialog' for step in steps))
                cast = steps.index(dict(op='cast', ref='spell', value=1))
                self.assertEqual(steps[cast-1], dict(op='choose_target', ref='source', player=1-seat))
                self.assertIn(dict(op='assert_zone_multiplicity', player=seat, zone=zone,
                                   index=self.rows[name]['csvid'], value=1), steps[cast:])
            steps = self.steps('Ydwen Efreet', f'replay_block_{seat}')
            capture = steps.index(dict(op='capture'))
            restore = steps.index(dict(op='restore'))
            observed = steps.index(dict(op='assert_recorded_state'))
            self.assertEqual(steps[capture+1:restore-1], steps[restore+2:observed])
            self.assertIn(dict(op='combat_damage'), steps[observed+1:])


if __name__ == '__main__':
    unittest.main()
