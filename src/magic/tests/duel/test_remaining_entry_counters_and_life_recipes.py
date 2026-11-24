import json
from copy import deepcopy
from pathlib import Path
import unittest

import remaining_entry_counters_and_life_recipes as recipe


class RemainingEntryCountersAndLifeRecipesTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.pins = recipe.profiles()
        all_pins = cls.pins['sources'] | cls.pins['witnesses']
        cls.oracle = {name: pin['oracle'] for name, pin in all_pins.items()}
        cls.rows = {name: pin['registry'] for name, pin in all_pins.items()}

    def render(self, name, variant):
        oracle = self.oracle[name]
        home = dict(family=recipe.FAMILY, version=2, params=recipe.classify(oracle, self.rows[name]['csvid']),
                    variants=[variant], oracle_text=oracle['oracle_text'])
        shape = recipe.spec(home, oracle)
        return home, recipe.expand(name, shape['csvid'], home, shape, [variant], self.rows, self.oracle)[0]

    def test_whole_oracle_and_typed_declaration(self):
        name = 'Icatian Moneychanger'
        home, _ = self.render(name, 'second_0')
        for wrong in (dict(home, family='other'), dict(home, version=2.0),
                      dict(home, params=dict(home['params'], csvid=1799.0)),
                      dict(home, variants=['second_0', 'second_0'])):
            with self.assertRaises(ValueError):
                recipe.spec(wrong, self.oracle[name])
        changed = dict(self.oracle[name], oracle_text=self.oracle[name]['oracle_text'].replace('your upkeep', 'an upkeep'))
        with self.assertRaises(ValueError):
            recipe.spec(home, changed)

    def test_witness_must_match_both_sources(self):
        recipe.validate_witnesses(self.oracle, self.rows)
        rows = deepcopy(self.rows)
        rows['Forest']['handler'] = 'card_dummy'
        with self.assertRaises(ValueError):
            recipe.validate_witnesses(self.oracle, rows)

    def test_both_seat_full_print_and_no_unsupported_decline(self):
        for pin in self.pins['sources'].values():
            oracle, row = pin['oracle'], pin['registry']
            shape = recipe.classify(oracle, row['csvid'])
            modes = recipe.variants(shape)
            self.assertIn('countered_0', modes)
            self.assertIn('countered_1', modes)
            for mode in modes:
                self.assertEqual(self.render(row['name'], mode)[0]['oracle_text'], oracle['oracle_text'])
        self.assertNotIn('decline_1', recipe.variants({'csvid': recipe.WALL}))
        self.assertNotIn('decline_1', recipe.variants({'csvid': recipe.SOULSTINGER}))

    def test_countered_never_runs_entry_choices(self):
        steps = self.render('Icatian Moneychanger', 'countered_0')[1]['steps']
        self.assertIn('cast_response', [step['op'] for step in steps])
        self.assertNotIn('upkeep_step', [step['op'] for step in steps])
        self.assertNotIn('activate', [step['op'] for step in steps])
        for seat in (0, 1):
            aura = self.render('Shape of the Wiitigo', f'countered_{seat}')[1]['steps']
            target = next(i for i, action in enumerate(aura) if action['op'] == 'choose_target')
            cast = next(i for i, action in enumerate(aura) if action['op'] == 'cast')
            self.assertLess(target, cast)
            self.assertEqual(aura[target]['ref'], 'host')
            self.assertEqual(aura[target]['player'], seat)

    def test_tapped_and_depleted_channeler_costs_are_distinct(self):
        steps = self.render('Channeler Initiate', 'empty_1')[1]['steps']
        activations = [step for step in steps if step['op'] == 'activate']
        observations = [step['value'] for step in steps
                        if step['op'] == 'assert_counter' and step['ref'] == 'source']
        self.assertEqual(len(activations), 3)
        self.assertEqual(observations, [3, 2, 1, 0])
        self.assertEqual([step for step in steps if step['op'] == 'can_activate'][-1]['value'], 0)

    def test_fresh_land_attacks_and_retains_mana_in_separate_case(self):
        steps = self.render('Wall of Resurgence', 'fresh_0')[1]['steps']
        play = next(i for i, step in enumerate(steps) if step['op'] == 'play_land')
        cast = next(i for i, step in enumerate(steps) if step['op'] == 'cast')
        attack = next(i for i, step in enumerate(steps) if step['op'] == 'attack')
        self.assertLess(play, cast)
        self.assertLess(cast, attack)
        self.assertEqual(steps[attack]['ref'], 'land')
        mana = self.render('Wall of Resurgence', 'mana_1')[1]['steps']
        self.assertTrue(any(step['op'] == 'activate' and step['ref'] == 'land' for step in mana))

    def test_every_home_exists_only_in_private_payload(self):
        data = Path(recipe.__file__).with_name('data')
        for name, pin in self.pins['sources'].items():
            csvid = pin['registry']['csvid']
            folder = data / f'cases/cards/{csvid // 100:03d}'
            homes = list(folder.glob(f'{csvid:05d}-*.json'))
            self.assertEqual(len(homes), 1)
            self.assertEqual(json.loads(homes[0].read_bytes())['card'], name)
