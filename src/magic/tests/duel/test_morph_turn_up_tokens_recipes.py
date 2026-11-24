"""Focused contracts for complete Morph token declarations and payments."""
import copy
import json
import unittest
from pathlib import Path

import morph_turn_up_tokens_recipes as model
from generate_cases import registry


class MorphTurnUpTokenRecipesTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        data = Path(__file__).parent / 'data'
        cls.oracle = json.loads((data / 'oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {row['name']: row for row in registry()}
        cls.sources = {}
        for name, pin in model.pins()['sources'].items():
            homes = list((data / 'cases/cards').glob(f"*/{pin['csvid']:05d}-*.json"))
            assert len(homes) == 1
            cls.sources[name] = json.loads(homes[0].read_text(encoding='utf8'))['recipes'][0]

    def case(self, name, variant):
        declaration = self.sources[name]
        record = self.oracle[name]
        shape = model.spec(declaration, record)
        return model.expand(name, shape['csvid'], declaration, shape, [variant], self.rows, self.oracle)[0]

    @staticmethod
    def token_counts(case, token, owner):
        return [step['value'] for step in case['steps']
                if step['op'] == 'assert_zone_multiplicity' and step['index'] == token
                and step['player'] == owner and step['zone'] == 'battlefield']

    def test_all_complete_records_and_each_controller(self):
        model.validate_witnesses(self.oracle, self.rows)
        self.assertEqual(sum(len(source['variants']) for source in self.sources.values()), 100)
        for name, source in self.sources.items():
            self.assertEqual(source['variants'], model.variants(model.mechanism(self.oracle[name])))
            self.assertEqual([suffix.rsplit('_', 1)[1] for suffix in source['variants'][::2]], ['0'] * (len(source['variants']) // 2))
            self.assertEqual([suffix.rsplit('_', 1)[1] for suffix in source['variants'][1::2]], ['1'] * (len(source['variants']) // 2))
            for variant in source['variants']:
                self.assertEqual(self.case(name, variant)['_recipe']['covered_clauses'], self.oracle[name]['oracle_text'].splitlines())

    def test_reject_partial_and_mutated_rules_and_pins(self):
        name = 'Ponyback Brigade'
        partial = dict(self.oracle[name], oracle_text=self.oracle[name]['oracle_text'].splitlines()[1])
        with self.assertRaises(ValueError):
            model.mechanism(partial)
        padded = dict(self.oracle[name], oracle_text=self.oracle[name]['oracle_text'] + '\nFlying')
        with self.assertRaises(ValueError):
            model.mechanism(padded)
        declaration = copy.deepcopy(self.sources[name])
        declaration['params']['mechanism']['morph'][0] = 1
        with self.assertRaises(ValueError):
            model.spec(declaration, self.oracle[name])
        changed = dict(self.rows, Snake=dict(self.rows['Snake'], handler='card_dummy'))
        with self.assertRaises(ValueError):
            model.validate_witnesses(self.oracle, changed)

    def test_warbreak_chooses_x_and_spends_twice_x_plus_red(self):
        for owner in (0, 1):
            for mode, x, maximum, remainder in [('x_zero', 0, 0, 0), ('x_one', 1, 2, 0), ('flip', 3, 6, 0), ('x_surplus', 2, 5, 1)]:
                case = self.case('Warbreak Trumpeter', f'{mode}_{owner}')
                steps = case['steps']
                index = steps.index(dict(op='activate', ref='source', value=1))
                self.assertEqual(steps[index-1], dict(op='choose_x', player=owner, value=x))
                self.assertIn(dict(op='set_mana', player=owner, color=0, value=maximum), steps[:index])
                self.assertIn(dict(op='assert_mana', player=owner, color=0, value=remainder), steps[index:])
                self.assertIn(dict(op='assert_mana', player=owner, color=4, value=0), steps[index:])
                self.assertEqual(self.token_counts(case, 18746, owner), [0, x])
                self.assertEqual(self.token_counts(case, 18746, 1-owner), [0, 0])

    def test_ponyback_enters_again_or_flips_once(self):
        for owner in (0, 1):
            self.assertEqual(self.token_counts(self.case('Ponyback Brigade', f'reenter_{owner}'), 18746, owner), [3, 3, 6])
            self.assertEqual(self.token_counts(self.case('Ponyback Brigade', f'repeat_{owner}'), 18746, owner), [0, 3, 3])
            self.assertEqual(self.token_counts(self.case('Ponyback Brigade', f'countered_{owner}'), 18746, owner), [0])

    def test_hermit_global_anthem_ends_when_source_dies(self):
        for owner in (0, 1):
            case = self.case('Thelonite Hermit', f'anthem_leaves_{owner}')
            self.assertEqual(self.token_counts(case, 18813, owner), [1, 5, 5])
            self.assertEqual(self.token_counts(case, 18813, 1-owner), [1, 1, 1])
            for player in (owner, 1-owner):
                for prop in ('power', 'toughness'):
                    self.assertEqual([s['value'] for s in case['steps'] if s.get('ref') == f'token_{player}_0' and s.get('property') == prop], [1, 2, 1])

    def test_hydra_paid_x_and_death_quantity(self):
        for owner in (0, 1):
            hardcast = self.case('Hooded Hydra', f'hardcast_death_{owner}')
            cost = [s for s in hardcast['steps'] if s['op'] in ('choose_dialog', 'choose_x')]
            self.assertEqual(cost, [dict(op='choose_dialog', player=owner, value=1), dict(op='choose_x', player=owner, value=2)])
            for mode, counts in [('faceup_zero', [0]), ('hidden_death', [0, 0]), ('hardcast_death', [0, 2]),
                                 ('flip_death', [0, 0, 5]), ('grown_flip_death', [0, 0, 0, 6]), ('flip_bounce', [0, 0, 0])]:
                self.assertEqual(self.token_counts(self.case('Hooded Hydra', f'{mode}_{owner}'), 18821, owner), counts)
            grown = self.case('Hooded Hydra', f'grown_flip_death_{owner}')
            self.assertEqual([s['value'] for s in grown['steps'] if s.get('ref') == 'source' and s.get('property') == 'plus1_counters'], [0, 1, 6])

    def test_no_choice_on_denied_flip_and_paid_offturn_flip(self):
        for name in self.sources:
            for owner in (0, 1):
                denial = self.case(name, f'flip_wrong_color_{owner}')['steps']
                self.assertIn(dict(op='activate', ref='source', value=0), denial)
                self.assertNotIn('choose_x', [step['op'] for step in denial])
                offturn = self.case(name, f'offturn_{owner}')['steps']
                self.assertLess(offturn.index(dict(op='advance_turn')), offturn.index(dict(op='activate', ref='source', value=1)))

    def test_replay_repeats_real_payment_and_exact_receipts(self):
        for name in self.sources:
            for owner in (0, 1):
                steps = self.case(name, f'replay_{owner}')['steps']
                actions = [step['op'] for step in steps]
                capture, record, roundtrip = [actions.index(op) for op in ('capture', 'record_state', 'ai_roundtrip')]
                self.assertEqual(steps[capture+1:record], steps[roundtrip+1:-1])
                self.assertIn('activate', actions[capture:record])
                self.assertEqual(actions[-1], 'assert_recorded_state')


if __name__ == '__main__':
    unittest.main()
