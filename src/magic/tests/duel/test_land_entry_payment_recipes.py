"""Focused contracts for exact land entry payment, mana and turn witnesses."""
import copy
import json
import unittest
from pathlib import Path

import land_entry_payment_recipes as recipe
from generate_cases import registry


class LandEntryPaymentRecipes(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        data = Path(__file__).parent / 'data'
        cls.oracle = json.loads((data / 'oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {row['name']: row for row in registry()}
        profile = recipe.pins()
        recipe.validate_witnesses(cls.oracle, cls.rows)
        cls.cases = {}
        cls.fee_lands = set()
        cls.pilot_lands = set()
        for name, pin in profile['sources'].items():
            csvid = pin['csvid']
            path, = (data / 'cases/cards').glob(f'*/{csvid:05d}-*.json')
            declaration, = json.loads(path.read_text(encoding='utf8'))['recipes']
            model = recipe.spec(declaration, cls.oracle[name])
            variants = recipe.variants(model)
            if declaration['variants'] != variants:
                raise AssertionError(name + ' did not enroll the complete reviewed matrix')
            if model['kind'] == 'fee':
                cls.fee_lands.add(name)
            if model['kind'] != 'energy':
                cls.pilot_lands.add(name)
            cls.cases[name] = {case['_recipe']['variant']: case for case in
                               recipe.expand(name, csvid, declaration, model, variants, cls.rows, cls.oracle)}

    def steps(self, name, mode, player):
        return self.cases[name][f'{mode}_{player}']['steps']

    def test_whole_oracle_rules_and_independent_witness_prices(self):
        self.assertEqual(self.fee_lands, {'Rupture Spire', 'Transguild Promenade', 'Gateway Plaza'})
        for name in sorted(self.pilot_lands):
            cells = self.cases[name]
            record = self.oracle[name]
            model = recipe.mechanism(record)
            self.assertEqual(model['gate'], name == 'Gateway Plaza')
            self.assertEqual(model['enter_tapped'], True)
            self.assertEqual(cells['color_g_0']['_recipe']['covered_clauses'], record['oracle_text'].splitlines())
            for text in (record['oracle_text'].replace('enters tapped', 'enters untapped'),
                         record['oracle_text'].replace('any color', 'any type'),
                         record['oracle_text'].replace('{1}', '{2}') if name in self.fee_lands else
                         record['oracle_text'].replace('{C}', '{U}')):
                with self.assertRaises(ValueError):
                    recipe.mechanism(dict(record, oracle_text=text))
        altered = copy.copy(self.oracle)
        altered['Soul Warden'] = dict(altered['Soul Warden'], mana_cost='{1}{W}')
        with self.assertRaisesRegex(ValueError, 'Soul Warden'):
            recipe.validate_witnesses(altered, self.rows)

    def test_all_cases_clear_the_seeded_pools_without_fabricating_mana(self):
        for cells in self.cases.values():
            for label, case in cells.items():
                actual = [s for s in case['steps'] if s['op'] == 'set_mana']
                self.assertEqual(actual, [dict(op='set_mana', player=p, color=c, value=0)
                                          for p in (0, 1) for c in range(6)], label)

    def test_fee_chooses_an_actual_source_or_existing_mana(self):
        for name in sorted(self.fee_lands):
            for player in (0, 1):
                for mode, giver in (('pay_wastes', 'Wastes'), ('pay_forest', 'Forest'), ('floating_forest', 'Forest')):
                    cell = self.cases[name][f'{mode}_{player}']
                    steps = cell['steps']
                    self.assertIn(dict(name=giver, ref='payer', player=player, zone='battlefield'), cell['setup'])
                    event = steps.index(dict(op='play_land', ref='source', value=1))
                    if mode == 'floating_forest':
                        self.assertLess(steps.index(dict(op='activate', ref='payer', value=1)), event)
                        self.assertEqual(dict(op='choose_mana', player=player, color=3) in steps[:event], player == 0)
                    else:
                        self.assertEqual(dict(op='choose_mana_source', ref='payer') in steps[:event], player == 0)
                    self.assertNotIn('choose_dialog', [s['op'] for s in steps])
                    self.assertIn(dict(op='assert_card', ref='payer', property='tapped', value=1), steps[event:])
                    self.assertIn(dict(op='assert_card', ref='source', property='tapped', value=1), steps[event:])

    def test_decline_and_insufficient_payment_sacrifice_the_land(self):
        for name in sorted(self.fee_lands):
            for player in (0, 1):
                for mode in ('unfunded', 'tapped_provider', 'opposing_provider', 'sacrifice_witness'):
                    steps = self.steps(name, mode, player)
                    self.assertEqual(dict(op='choose_cancel_mana', player=player) in steps, player == 0)
                    self.assertIn(dict(op='assert_zone_multiplicity', player=player, zone='graveyard', value=1,
                                       index=self.rows[name]['csvid']), steps)
                self.assertIn(dict(op='assert_card', ref='witness', property='plus1_counters', value=1),
                              self.steps(name, 'sacrifice_witness', player))
            declined = self.cases[name]['decline_0']
            self.assertIn(dict(name='Forest', ref='payer', player=0, zone='battlefield'), declined['setup'])
            self.assertIn(dict(op='choose_cancel_mana', player=0), declined['steps'])
            self.assertIn(dict(op='assert_card', ref='payer', property='tapped', value=0), declined['steps'])
            self.assertNotIn('decline_1', self.cases[name])

    def test_persistent_land_quota_and_actual_untap(self):
        for name in sorted(self.pilot_lands):
            for player in (0, 1):
                mode = self.steps(name, 'next_turn_slot', player)
                played = mode.index(dict(op='play_land', ref='source', value=1))
                denied = mode.index(dict(op='play_land', ref='ordinary', value=0))
                untap = mode.index(dict(op='untap_step', player=player))
                later = mode.index(dict(op='play_land', ref='ordinary', value=1))
                self.assertTrue(played < denied < untap < later)
                self.assertEqual(sum(s['op'] == 'advance_turn' for s in mode), 2)
                scout = self.steps(name, 'scout_put', player)
                self.assertLess(scout.index(dict(op='activate', ref='scout', value=1)),
                                scout.index(dict(op='play_land', ref='ordinary', value=1)))
                self.assertIn(dict(op='play_land', ref='extra', value=0), scout)
                self.assertNotIn(dict(op='play_land', ref='source', value=1), scout)
                self.assertIn(dict(op='play_land', ref='source', value=0), self.steps(name, 'off_turn', player))
                if name in self.fee_lands:
                    self.assertIn(dict(op='play_land', ref='ordinary', value=0), self.steps(name, 'unfunded', player))

    def test_five_colors_use_actual_mana_and_delayed_vestige_uses_colorless(self):
        for name in sorted(self.pilot_lands):
            for player in (0, 1):
                for color in recipe.COLORS[1:]:
                    cell = self.cases[name][f'color_{color.lower()}_{player}']
                    steps = cell['steps']
                    self.assertIn(dict(name=recipe.SPENDERS[color], ref='spender', player=player, zone='hand'),
                                  cell['setup'])
                    intent = steps.index(dict(op='choose_color', player=player, value=recipe.COLORS.index(color)))
                    spent = steps.index(dict(op='cast', ref='spender', value=1))
                    event = steps.index(dict(op='play_land', ref='source', value=1))
                    self.assertEqual(intent < event, name == 'Crumbling Vestige')
                    self.assertIn(dict(op='assert_mana', player=player, color=recipe.COLORS.index(color), value=1),
                                  steps[intent:spent])
                    if name in self.fee_lands:
                        self.assertLess(steps.index(dict(op='untap_step', player=player)), intent)
        for player in (0, 1):
            later = self.steps('Crumbling Vestige', 'later_colorless', player)
            untap = later.index(dict(op='untap_step', player=player))
            self.assertNotIn('choose_color', [s['op'] for s in later[untap:]])
            self.assertIn(dict(op='assert_mana', player=player, color=0, value=1), later[untap:])
            self.assertIn(dict(op='cast', ref='colorless_spender', value=1), later[untap:])

    def test_real_gatekeepers_distinguish_gate_and_non_gate_printings(self):
        for name in sorted(self.fee_lands):
            for player in (0, 1):
                steps = self.steps(name, 'gate_witness', player)
                untap = steps.index(dict(op='untap_step', player=player))
                cast = steps.index(dict(op='cast', ref='gatekeepers', value=1))
                for provider in ('generic_mana', 'payer', 'green_mana'):
                    self.assertIn(dict(op='activate', ref=provider, value=1), steps[untap:cast])
                self.assertIn(dict(op='assert_life', player=player, value=27 if name == 'Gateway Plaza' else 20),
                              steps[cast:])

    def test_ready_snapshot_replays_real_choices(self):
        for name in self.cases:
            for player in (0, 1):
                steps = self.steps(name, 'ready_replay', player)
                ops = [s['op'] for s in steps]
                self.assertLess(ops.index('capture'), ops.index('play_land'))
                self.assertEqual(steps[ops.index('capture')+1:ops.index('record_state')],
                                 steps[ops.index('ai_roundtrip')+2:ops.index('assert_recorded_state')])

    def test_hub_whole_oracle_and_untapped_entry_energy(self):
        record = self.oracle['Aether Hub']
        model = recipe.mechanism(record)
        self.assertEqual(model, dict(kind='energy', enter_tapped=False, entry_energy=1,
                                     energy_fee=1, tap_colors='CBUGRW'))
        for text in (record['oracle_text'].replace('Pay {E}', 'Pay {E}{E}'),
                     record['oracle_text'].replace('you get {E}', 'each player gets {E}'),
                     'This land enters tapped.\n' + record['oracle_text']):
            with self.assertRaises(ValueError):
                recipe.mechanism(dict(record, oracle_text=text))
        for player in (0, 1):
            steps = self.steps('Aether Hub', 'entry', player)
            play = steps.index(dict(op='play_land', ref='source', value=1))
            self.assertIn(dict(op='assert_energy', player=player, value=0), steps[:play])
            self.assertIn(dict(op='assert_energy', player=player, value=1), steps[play:])
            self.assertIn(dict(op='assert_energy', player=1-player, value=0), steps[play:])
            self.assertIn(dict(op='assert_card', ref='source', property='tapped', value=0), steps[play:])
            self.assertNotIn('choose_color', [s['op'] for s in steps])

    def test_hub_five_paid_colors_and_nonpaying_colorless(self):
        name = 'Aether Hub'
        for player in (0, 1):
            for color in recipe.COLORS[1:]:
                steps = self.steps(name, 'color_' + color.lower(), player)
                index = recipe.COLORS.index(color)
                choice = steps.index(dict(op='choose_color', player=player, value=index))
                tap = steps.index(dict(op='activate', ref='source', value=1))
                self.assertLess(steps.index(dict(op='play_land', ref='source', value=1)), choice)
                self.assertLess(choice, tap)
                self.assertIn(dict(op='assert_energy', player=player, value=1), steps[:tap])
                self.assertIn(dict(op='assert_energy', player=player, value=0), steps[tap:])
                self.assertIn(dict(op='assert_mana', player=player, color=index, value=1), steps[tap:])
                self.assertIn(dict(op='cast', ref='spender', value=1), steps[tap:])
            free = self.steps(name, 'free_colorless', player)
            choice = free.index(dict(op='choose_color', player=player, value=0))
            self.assertNotIn(dict(op='assert_energy', player=player, value=0), free[choice:])
            self.assertIn(dict(op='assert_mana', player=player, color=0, value=1), free[choice:])
            self.assertIn(dict(op='cast', ref='free_spender', value=1), free[choice:])

    def test_hub_depletion_and_opposing_energy_are_independent(self):
        name = 'Aether Hub'
        for player in (0, 1):
            for mode in ('spent_then_colorless', 'opposing_energy'):
                steps = self.steps(name, mode, player)
                untap = steps.index(dict(op='untap_step', player=player))
                later = steps[untap:]
                self.assertNotIn('choose_color', [s['op'] for s in later])
                self.assertIn(dict(op='cast', ref='white_spender', value=0), later)
                self.assertIn(dict(op='activate', ref='source', value=1), later)
                self.assertIn(dict(op='assert_energy', player=player, value=0), later)
                self.assertIn(dict(op='assert_energy', player=1-player, value=3 if mode=='opposing_energy' else 0),
                              later)
                self.assertIn(dict(op='assert_mana', player=player, color=0, value=1), later)
            own = self.steps(name, 'own_reserve', player)
            donor = own.index(dict(op='cast', ref='donor', value=1))
            self.assertLess(own.index(dict(op='activate', ref='ring', value=1)), donor)
            self.assertIn(dict(op='assert_energy', player=player, value=4), own[donor:])
            self.assertIn(dict(op='assert_energy', player=player, value=2), own[donor:])

    def test_hub_live_cast_payment_and_actual_entry_replay(self):
        name = 'Aether Hub'
        for player in (0, 1):
            paid = self.steps(name, 'colored_cast', player)
            cast = paid.index(dict(op='cast', ref='spender', value=1))
            self.assertEqual(dict(op='choose_mana_source', ref='source') in paid[:cast], player==0)
            self.assertIn(dict(op='assert_energy', player=player, value=0), paid[cast:])
            self.assertIn(dict(op='assert_card', ref='source', property='tapped', value=1), paid[cast:])
            reentered = self.steps(name, 'reenter', player)
            self.assertIn(dict(op='choose_target', player=player, ref='source'), reentered)
            self.assertIn(dict(op='play_land', ref='source', value=0), reentered)
            self.assertEqual(reentered.count(dict(op='play_land', ref='source', value=1)), 2)
            self.assertIn(dict(op='assert_energy', player=player, value=2), reentered)
            scout = self.steps(name, 'scout_entry', player)
            self.assertIn(dict(op='activate', ref='scout', value=1), scout)
            self.assertIn(dict(op='play_land', ref='ordinary', value=1), scout)
            self.assertIn(dict(op='play_land', ref='extra', value=0), scout)


if __name__ == '__main__':
    unittest.main()
