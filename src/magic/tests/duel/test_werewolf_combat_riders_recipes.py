"""Focused guards for full transform identities and combat rider fixtures."""
import json
from pathlib import Path
import unittest

from generate_cases import registry
import werewolf_combat_riders_recipes as model


class WerewolfCombatRiderTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        data = Path(__file__).parent / 'data'
        cls.oracle = json.loads((data / 'oracle.json').read_bytes())['cards']
        cls.rows = {row['name']: row for row in registry()}
        cls.homes = {}
        for card, pin in model.profiles()['sources'].items():
            home, = (data / 'cases/cards').glob(f"*/{pin['registry']['csvid']:05d}-*.json")
            cls.homes[card] = json.loads(home.read_bytes())['recipes'][0]

    def steps(self, card, mode, seat=0):
        declaration = self.homes[card]
        shape = model.spec(declaration, self.oracle[card])
        return model.build(card, self.rows[card]['csvid'], declaration, shape, f'{mode}_{seat}',
                           self.rows, self.oracle)['steps']

    def values(self, steps, op, **where):
        return [step['value'] for step in steps if step['op'] == op
                and all(step.get(key) == value for key, value in where.items())]

    def test_pins_and_front_homes(self):
        model.validate_witnesses(self.oracle, self.rows)
        self.assertEqual({self.rows[name]['csvid'] for name in self.homes},
                         {11958, 11980, 12208, 12000, 15755, 15826, 15879, 16015})
        for card, declaration in self.homes.items():
            front = self.oracle[card]
            self.assertEqual(declaration['oracle_text'], front['selected_face']['oracle_text'])
            self.assertEqual(declaration['variants'], model.variants(model.spec(declaration, front)))
            back = model.profiles()['sources'][card]['back_oracle']
            with self.assertRaises(ValueError):
                model.spec(declaration, back)

    def test_actual_two_seat_spell_history(self):
        for seat in (0, 1):
            for card in self.homes:
                csvid = self.rows[card]['csvid']
                for mode, face in [('front_split', csvid), ('back_split', csvid+1), ('back_two', csvid)]:
                    steps = self.steps(card, mode, seat)
                    self.assertEqual([s['ref'] for s in steps if s['op'] == 'cast'][0], 'source')
                    self.assertEqual(self.values(steps, 'cast'), [1, 1, 1])
                    self.assertEqual(self.values(steps, 'assert_card', ref='source', property='csvid')[-1], face)
                    self.assertEqual([s['player'] for s in steps if s['op'] == 'upkeep_step'][-1],
                                     seat if mode == 'front_split' else 1-seat)

    def test_mandatory_declarations(self):
        for mode, expected in [('defender', 1), ('required', 0), ('tapped_required', 1)]:
            steps = self.steps('Hanweir Watchkeep', mode)
            self.assertEqual(self.values(steps, 'can_finish_attackers'), [expected, 1])
        for mode, expected in [('front_omission', 0), ('auto_block', 1), ('tapped_blocker', 0), ('no_blocker', 0)]:
            steps = self.steps('Hinterland Hermit', mode)
            self.assertEqual(self.values(steps, 'assert_card', ref='source', property='blocked')[-1], expected)
            start = next(i for i, step in enumerate(steps) if step['op'] == 'begin_blockers')
            if mode == 'auto_block':
                self.assertEqual(steps[start-1], {'op':'choose_target', 'ref':'blocker', 'player':1})
            if mode == 'tapped_blocker':
                self.assertEqual(self.values(steps, 'choose_dialog'), [0])

    def test_blocker_filters_and_multiple_declarations(self):
        for mode, expected in [('front_block', 1), ('other_color', 0), ('same_color', 1), ('artifact', 1)]:
            self.assertEqual(self.values(self.steps('Gatstaf Shepherd', mode), 'block'), [expected])
        for card in ('Gatstaf Arsonists', 'Village Messenger'):
            for mode, expected in [('front_single', [1]), ('back_single', [0]), ('back_pair', [1, 1])]:
                self.assertEqual(self.values(self.steps(card, mode), 'block'), expected)
        for mode, expected in [('tribal_single', [0]), ('tribal_pair', [1, 1]), ('other_creature', [1]), ('opponent_wolf', [1])]:
            self.assertEqual(self.values(self.steps('Kruin Outlaw', mode), 'block'), expected)

    def test_combat_damage_witnesses(self):
        for card, mode, seat, life in [('Kruin Outlaw', 'back_double', 1, 14),
                                        ('Breakneck Rider', 'back_team', 0, 14),
                                        ('Breakneck Rider', 'front_team', 0, 20),
                                        ('Village Messenger', 'fresh_attack', 1, 19)]:
            steps = self.steps(card, mode, seat)
            self.assertEqual(self.values(steps, 'assert_life', player=1-seat)[-1], life)
            self.assertTrue(any(s['op'] == 'combat_damage' for s in steps))
            self.assertTrue(any(s['op'] == 'end_combat' for s in steps))
        for seat in (0, 1):
            fresh = self.steps('Village Messenger', 'fresh_attack', seat)
            self.assertEqual(self.values(fresh, 'assert_card', ref='source', property='sick'), [0, 0, 0])
            self.assertEqual(self.values(fresh, 'attack', ref='source'), [1])
            self.assertFalse(any(step['op'] == 'advance_turn' for step in fresh))
        first = self.steps('Kruin Outlaw', 'front_first')
        self.assertEqual(self.values(first, 'assert_card', ref='source', property='damage')[-1], 0)
        boosted = self.steps('Breakneck Rider', 'back_team', 1)
        needles = {'source':5, 'ally':3, 'idle':2}
        for ref, amount in needles.items():
            self.assertIn(amount, self.values(boosted, 'assert_card', ref=ref, property='power'))
        self.assertEqual(self.values(boosted, 'assert_card', ref='source', property='power')[-1], 4)
        self.assertEqual(self.values(boosted, 'assert_card', ref='ally', property='power')[-1], 2)

    def test_ready_front_needs_own_power_four(self):
        for mode, value in [('no_ally', 0), ('small_ally', 0), ('enemy_four', 0), ('own_four', 1), ('back_ready', 1)]:
            steps = self.steps('Lambholt Pacifist', mode, 1)
            self.assertEqual(self.values(steps, 'attack', ref='source'), [value])
            self.assertEqual(self.values(steps, 'assert_card', ref='source', property='sick')[-1], 0)


if __name__ == '__main__':
    unittest.main()
