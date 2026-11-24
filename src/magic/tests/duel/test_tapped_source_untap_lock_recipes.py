"""Whole-card and phase-boundary guards for tapped-source restrictions."""
import json
import unittest
from pathlib import Path

import tapped_source_untap_lock_recipes as model
from generate_cases import OPS, PROPERTIES, registry


class TappedSourceUntapLockTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle = json.loads((Path(__file__).parent / 'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {row['name']: row for row in registry()}

    def model(self, name):
        oracle = self.oracle[name]
        declaration = dict(family=model.FAMILY, version=2, oracle_text=oracle['oracle_text'],
                           params=dict(schema=1, mechanism=model.mechanism(oracle)))
        return declaration, model.spec(declaration, oracle)

    def case(self, name, mode, owner=0):
        declaration, profile = self.model(name)
        return model.expand(name, profile['csvid'], declaration, profile, [f'{mode}_{owner}'], self.rows, self.oracle)[0]

    def test_independent_type_scope_and_cost_rows(self):
        expected = {
            'Amber Prison': ('artifact, creature, or land', 4, False, True, [4, 0, 0, 0, 0, 0]),
            "Deserter's Quarters": ('creature', 6, False, True, [2, 0, 0, 0, 0, 0]),
            'Phyrexian Gremlins': ('artifact', 0, True, False, [2, 1, 0, 0, 0, 0]),
            'Rust Tick': ('artifact', 1, True, True, [3, 0, 0, 0, 0, 0]),
            'Mole Worms': ('land', 0, True, False, [2, 1, 0, 0, 0, 0]),
            'Mana Leech': ('land', 0, True, False, [2, 1, 0, 0, 0, 0]),
        }
        self.assertEqual(set(model.pins()['sources']), set(expected))
        for name, values in expected.items():
            _, parsed = self.model(name)
            self.assertEqual(tuple(parsed[k] for k in ('scope', 'activation_mana', 'creature', 'artifact', 'mana')), values)
            with self.assertRaises(ValueError):
                model.mechanism(dict(self.oracle[name], oracle_text=self.oracle[name]['oracle_text'] + '\nDraw a card.'))
        self.assertEqual(model.mechanism(self.oracle['Mana Leech']), model.mechanism(self.oracle['Mole Worms']))
        self.assertEqual(self.rows['Mana Leech']['handler'], self.rows['Mole Worms']['handler'])
        self.assertEqual(self.rows['Mana Leech']['csvid'], 3824)
        self.assertNotEqual(self.oracle['Mana Leech']['oracle_id'], self.oracle['Mole Worms']['oracle_id'])

    def test_paid_tap_is_before_target_effect_and_mana_is_spent(self):
        for name in model.pins()['sources']:
            for owner in (0, 1):
                steps = self.case(name, 'lifecycle', owner)['steps']
                activate = next(i for i, s in enumerate(steps) if s['op'] == 'activate')
                resolve = next(i for i in range(activate, len(steps)) if steps[i]['op'] == 'resolve')
                pending = steps[activate:resolve]
                self.assertEqual(steps[activate-1], dict(op='choose_target', player=owner, ref='target'))
                self.assertIn(dict(op='assert_card', ref='source', property='tapped', value=1), pending)
                self.assertIn(dict(op='assert_card', ref='target', property='tapped', value=0), pending)
                self.assertIn(dict(op='assert_mana', player=owner, color=0, value=0), pending)
                self.assertIn(dict(op='assert_card', ref='target', property='tapped', value=1), steps[resolve:])

    def test_real_phase_paths_distinguish_human_choice_and_native_ai(self):
        for name in model.pins()['sources']:
            keep = self.case(name, 'lifecycle')['steps']
            self.assertIn(dict(op='choose_priority', player=0, value=-2), keep)
            release = self.case(name, 'choose_to_untap')['steps']
            decision = next(i for i, s in enumerate(release) if s['op'] == 'choose_priority')
            self.assertEqual(release[decision], dict(op='choose_priority', player=0, ref='source'))
            self.assertEqual(release[decision+1], dict(op='untap_step', player=0))
            self.assertIn(dict(op='assert_card', ref='source', property='tapped', value=0), release[decision:])
            self.assertIn(dict(op='assert_card', ref='target', property='tapped', value=0), release[decision:])
            self.assertFalse(any(s['op'] == 'choose_priority' for s in self.case(name, 'lifecycle', 1)['steps']))

    def test_manual_untap_releases_only_if_it_is_source(self):
        for label, actual in [('manual_target', 1), ('manual_source', 0)]:
            steps = self.case('Amber Prison', label)['steps']
            twiddles = [s['value'] for s in steps if s['op'] == 'choose_dialog']
            self.assertEqual(twiddles, [1, 0])
            final = max(i for i, s in enumerate(steps) if s['op'] == 'untap_step')
            self.assertIn(dict(op='assert_card', ref='target', property='tapped', value=actual), steps[final:])

    def test_fresh_sick_and_unaffordable_actions_queue_no_target(self):
        for name in ('Rust Tick', 'Phyrexian Gremlins', 'Mole Worms'):
            steps = self.case(name, 'fresh')['steps']
            self.assertIn(dict(op='activate', ref='source', value=0), steps)
            self.assertFalse(any(s['op'] in ('advance_turn', 'choose_target') for s in steps))
        for name in ('Amber Prison', "Deserter's Quarters", 'Rust Tick'):
            steps = self.case(name, 'insufficient_activation')['steps']
            self.assertIn(dict(op='activate', ref='source', value=0), steps)
            self.assertFalse(any(s['op'] == 'choose_target' for s in steps))

    def test_scope_variants_include_each_printed_target_type(self):
        expected = {'Amber Prison': ['Ornithopter', 'Grizzly Bears', 'Forest'],
                    "Deserter's Quarters": ['Grizzly Bears', 'Ornithopter'],
                    'Phyrexian Gremlins': ['Ornithopter', 'Darksteel Citadel'],
                    'Rust Tick': ['Ornithopter', 'Darksteel Citadel'],
                    'Mole Worms': ['Forest', 'Darksteel Citadel'],
                    'Mana Leech': ['Forest', 'Darksteel Citadel']}
        for name, targets in expected.items():
            for index, target in enumerate(targets):
                case = self.case(name, f'target_{index}' if index else 'lifecycle')
                self.assertEqual(next(s['name'] for s in case['setup'] if s['ref'] == 'target'), target)
        no_target = self.case("Deserter's Quarters", 'no_legal_target')
        self.assertEqual([s['name'] for s in no_target['setup'] if s['zone'] == 'battlefield'], ['Darksteel Citadel'])

    def test_counterspell_uses_native_parent_and_bounce_keeps_ownership(self):
        for name in model.pins()['sources']:
            for owner in (0, 1):
                countered = self.case(name, 'cast_countered', owner)['steps']
                response = next(i for i, s in enumerate(countered) if s['op'] == 'cast_response')
                self.assertEqual(countered[response-1], dict(op='announce_spell', ref='source'))
                self.assertFalse(any(s['op'] in ('activate', 'choose_target') for s in countered))
                bounced = self.case(name, 'source_bounce', owner)['steps']
                self.assertIn(dict(op='assert_zone_multiplicity', player=owner, zone='hand', index=self.rows[name]['csvid'], value=1), bounced)

    def test_all_cases_observe_whole_oracle_and_replay_the_same_real_steps(self):
        model.validate_witnesses(self.oracle, self.rows)
        count = 0
        for name in model.pins()['sources']:
            declaration, profile = self.model(name)
            cases = model.expand(name, profile['csvid'], declaration, profile, model.variants(profile), self.rows, self.oracle)
            count += len(cases)
            for case in cases:
                self.assertEqual(case['_recipe']['covered_clauses'], self.oracle[name]['oracle_text'].splitlines())
                steps = case['steps']
                for i, s in enumerate(steps):
                    self.assertIn(s['op'], OPS)
                    if 'property' in s:
                        self.assertIn(s['property'], PROPERTIES)
                    if s['op'] == 'untap_step':
                        self.assertTrue(any(prior['op'] == 'advance_turn' for prior in steps[max(i-2, 0):i]))
                if 'replay' in case['name']:
                    ops = [s['op'] for s in steps]
                    self.assertEqual(steps[ops.index('capture')+1:ops.index('record_state')], steps[ops.index('restore')+1:-1])
        self.assertEqual(count, 190)
