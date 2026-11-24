"""Focused guards for paid Level Up and actual combat rider skeletons."""
import json
from pathlib import Path
import unittest

from generate_cases import registry
import level_keyword_combat_recipes as model


class LevelKeywordCombatTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        data = Path(__file__).parent / 'data'
        cls.oracle = json.loads((data / 'oracle.json').read_bytes())['cards']
        cls.rows = {row['name']: row for row in registry()}
        cls.homes = {}
        for card, pin in model.profiles()['sources'].items():
            home, = (data / 'cases/cards').glob(f"*/{pin['registry']['csvid']:05d}-*.json")
            cls.homes[card] = json.loads(home.read_bytes())['recipes'][0]

    def case(self, card, mode, seat=0):
        home = self.homes[card]
        shape = model.spec(home, self.oracle[card])
        return model.build(card, self.rows[card]['csvid'], home, shape, f'{mode}_{seat}', self.rows, self.oracle)

    def values(self, case, op, **fields):
        return [step['value'] for step in case['steps'] if step['op'] == op
                and all(step.get(key) == value for key, value in fields.items())]

    def test_pinned_sources_and_bounds(self):
        model.validate_witnesses(self.oracle, self.rows)
        self.assertEqual({self.rows[card]['csvid'] for card in self.homes},
                         {10878, 10887, 10946, 10947, 10956, 10970, 11041, 11056})
        for card, home in self.homes.items():
            record = self.oracle[card]
            shape = model.spec(home, record)
            self.assertEqual(home['oracle_text'], record['oracle_text'])
            self.assertEqual(home['variants'], model.variants(shape))
            self.assertGreaterEqual(len(home['variants']), 8)
            self.assertLessEqual(len(home['variants']), 16)
            changed = dict(record, oracle_text=record['oracle_text'] + '\nVigilance')
            with self.assertRaises(ValueError):
                model.spec(home, changed)

    def test_activation_fees_and_pending_band_transition(self):
        for card, home in self.homes.items():
            shape = model.spec(home, self.oracle[card])
            threshold = shape['second'][0]
            case = self.case(card, 'thresholds', 1)
            paid = self.values(case, 'activate', ref='source')
            self.assertEqual(paid, [1]*(threshold+1))
            self.assertEqual(self.values(case, 'assert_stack').count(1), len(paid)+1)
            self.assertIn(shape['first'][1], self.values(case, 'assert_card', ref='source', property='power'))
            self.assertEqual(self.values(case, 'assert_card', ref='source', property='power')[-1], shape['second'][1])
            steps = case['steps']
            for index, step in enumerate(steps):
                if step['op'] != 'activate':
                    continue
                expected = shape['activation']
                self.assertEqual(steps[index-8:index-2],
                                 [dict(op='set_mana', player=1, color=c, value=n) for c,n in enumerate(expected)])
                resolve = next(i for i in range(index+1,len(steps)) if steps[i]['op'] == 'resolve')
                self.assertTrue(self.values({'steps':steps[index:resolve]}, 'assert_card',
                                            ref='source', property='toughness'))

    def test_denial_paths(self):
        for mode, actual in [('short_payment', [0]), ('offturn', []), ('pending_spell', [])]:
            case = self.case('Student of Warfare', mode, 0)
            self.assertEqual(self.values(case, 'activate', ref='source'), actual)
            self.assertEqual(self.values(case, 'require_can_activate', ref='source'), [0, 1] if mode == 'pending_spell' else [0])
            self.assertEqual(self.values(case, 'assert_card', ref='source', property='power')[-1], 1)

    def test_combat_legality_and_tapping(self):
        for card in ('Knight of Cliffhaven', 'Skywatcher Adept'):
            for mode, allowed in [('base_block', 1), ('first_ground', 0), ('second_ground', 0), ('second_reach', 1)]:
                case = self.case(card, mode, 1)
                self.assertEqual(self.values(case, 'block'), [allowed])
                vigilant = card == 'Knight of Cliffhaven' and mode.startswith('second_')
                self.assertEqual(self.values(case, 'assert_card', ref='source', property='tapped')[0], int(not vigilant))
        for mode, taps in [('base_attack', 1), ('first_attack', 0), ('second_attack', 0)]:
            self.assertEqual(self.values(self.case('Ikiral Outrider', mode), 'assert_card', ref='source', property='tapped')[0], taps)

    def test_islandwalk_and_shroud_use_live_witnesses(self):
        for seat, defense in [(0,'Island'),(1,'Tropical Island')]:
            for mode, allowed in [('first_defense_island', 1), ('second_defense_island', 0),
                                  ('second_own_island', 1), ('second_no_island', 1)]:
                case = self.case('Halimar Wavewatch', mode, seat)
                self.assertEqual(self.values(case, 'block'), [allowed])
                if mode == 'second_defense_island':
                    land, = [item for item in case['setup'] if item['ref'] == 'land']
                    self.assertEqual((land['name'], land['player']), (defense, 1-seat))
        hada = self.case('Hada Spy Patrol', 'target_gate')
        for seat in (0,1):
            self.assertEqual(self.values(hada, 'can_cast', ref=f'bounce_{seat}'), [1, 0])
        self.assertEqual(self.values(self.case('Hada Spy Patrol', 'base_block'), 'block'), [1])
        self.assertEqual(self.values(self.case('Hada Spy Patrol', 'first_block'), 'block'), [0])
        self.assertEqual(self.values(self.case('Hada Spy Patrol', 'second_block'), 'block'), [0])

    def test_damage_proves_trample_and_strikes(self):
        for mode, life in [('first_block', 20), ('second_block', 16)]:
            case = self.case('Beastbreaker of Bala Ged', mode, 0)
            self.assertEqual(self.values(case, 'assert_life', player=1)[-1], life)
        for mode, received in [('first_block', 1), ('second_block', 0)]:
            case = self.case('Caravan Escort', mode, 1)
            self.assertEqual(self.values(case, 'assert_card', ref='source', property='damage')[-1], received)
        student = self.case('Student of Warfare', 'second_attack', 1)
        self.assertEqual(self.values(student, 'assert_life', player=0)[-1], 12)


if __name__ == '__main__':
    unittest.main()
