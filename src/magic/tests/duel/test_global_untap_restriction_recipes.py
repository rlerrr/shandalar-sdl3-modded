"""Contracts for the global untap restriction pilot and its independent witnesses."""
import json
import unittest
from pathlib import Path

import global_untap_restriction_recipes as model
from generate_cases import OPS, PROPERTIES, registry


class GlobalUntapRestrictionTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle = json.loads((Path(__file__).parent / 'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {row['name']: row for row in registry()}

    def declaration(self, name):
        record = self.oracle[name]
        return dict(family=model.FAMILY, version=2, oracle_text=record['oracle_text'],
                    params=dict(schema=1, mechanism=model.mechanism(record)))

    def case(self, name, mode, owner):
        declaration = self.declaration(name)
        profile = model.spec(declaration, self.oracle[name])
        return model.expand(name, profile['csvid'], declaration, profile, [f'{mode}_{owner}'], self.rows, self.oracle)[0]

    def test_independent_pilot_types_mana_and_mechanisms(self):
        expected = {
            'Back to Basics': ('nonbasic_land', False, False, [2, 0, 1, 0, 0, 0]),
            'Choke': ('island', False, False, [2, 0, 0, 1, 0, 0]),
            'Crackdown': ('nonwhite_high', False, False, [2, 0, 0, 0, 0, 1]),
            'Juntu Stakes': ('low', False, False, [2, 0, 0, 0, 0, 0]),
            'Marble Titan': ('high', False, True, [3, 0, 0, 0, 0, 1]),
            'Meekstone': ('high', False, False, [1, 0, 0, 0, 0, 0]),
            'Curse of Marit Lage': ('island', True, False, [3, 0, 0, 0, 2, 0]),
            'Wrath of Marit Lage': ('red', True, False, [3, 0, 2, 0, 0, 0]),
        }
        self.assertEqual(set(expected), set(model.pins()['sources']))
        model.validate_witnesses(self.oracle, self.rows)
        for name, values in expected.items():
            p = model.spec(self.declaration(name), self.oracle[name])
            self.assertEqual(tuple(p[k] for k in ('scope', 'entry', 'creature', 'mana')), values)
            with self.assertRaises(ValueError):
                model.spec(self.declaration(name), dict(self.oracle[name], mana_cost='{0}'))
            with self.assertRaises(ValueError):
                model.mechanism(dict(self.oracle[name], oracle_text=self.oracle[name]['oracle_text'] + '\nDraw a card.'))

    def test_predicate_boundaries(self):
        examples = {
            'nonbasic_land': (['Tropical Island', 'Dryad Arbor', 'Darksteel Citadel'], ['Wastes', 'Island', 'Sol Ring']),
            'island': (['Island', 'Tropical Island'], ['Forest', 'Wastes', 'Sol Ring']),
            'nonwhite_high': (['Hill Giant', 'Juggernaut'], ['Goblin Piker', 'Boros Reckoner', 'Serra Angel', 'Sol Ring']),
            'low': (['Ornithopter', 'Raging Goblin', 'Dryad Arbor'], ['Grizzly Bears', 'Sol Ring']),
            'high': (['Hill Giant', 'Serra Angel', 'Juggernaut'], ['Goblin Piker', 'Sol Ring']),
            'red': (['Hill Giant', 'Goblin Piker', 'Boros Reckoner'], ['Grizzly Bears', 'Juggernaut', 'Sol Ring']),
        }
        for scope, (matches, misses) in examples.items():
            for name in matches:
                self.assertTrue(model.restricted(self.oracle[name], scope), (name, scope))
            for name in misses:
                self.assertFalse(model.restricted(self.oracle[name], scope), (name, scope))

    def test_entry_taps_matching_permanents_only_and_static_lock_does_not_tap(self):
        for name in model.pins()['sources']:
            for owner in (0, 1):
                case = self.case(name, 'untapped_entry', owner)
                p = model.mechanism(self.oracle[name])
                cut = next(i for i, s in enumerate(case['steps']) if s['op'] == 'advance_turn')
                before_turn = case['steps'][:cut]
                self.assertFalse(any(s['op'].startswith('choose_') for s in before_turn))
                for host in case['setup']:
                    if host['ref'].startswith('host_'):
                        tapped = p['entry'] and model.restricted(self.oracle[host['name']], p['scope'])
                        self.assertIn(dict(op='assert_card', ref=host['ref'], property='tapped', value=int(tapped)), before_turn)

    def test_untap_steps_are_real_and_checks_cover_both_players(self):
        for name in model.pins()['sources']:
            for owner in (0, 1):
                actions = self.case(name, 'source_removed', owner)['steps']
                self.assertEqual([s['player'] for s in actions if s['op'] == 'untap_step'], [1-owner, owner] * 2)
                for i, action in enumerate(actions):
                    if action['op'] == 'untap_step':
                        self.assertEqual(actions[i-1]['op'], 'advance_turn')
                key = dict(op='assert_card', ref=f'host_{owner}_0', property='tapped')
                self.assertIn(dict(key, value=1), actions)
                self.assertIn(dict(key, value=0), actions)

    def test_power_change_crosses_cutoff_via_real_aura(self):
        for name, before, after in [('Crackdown', 2, 4), ('Juntu Stakes', 1, 3), ('Marble Titan', 2, 4), ('Meekstone', 2, 4)]:
            steps = self.case(name, 'power_change', 0)['steps']
            split = next(i for i, s in enumerate(steps) if s['op'] == 'cast' and s['ref'] == 'boost')
            self.assertIn(dict(op='choose_target', player=0, ref='host_0_0'), steps[:split])
            self.assertIn(dict(op='assert_card', ref='host_0_0', property='power', value=before), steps[:split])
            self.assertIn(dict(op='assert_card', ref='host_0_0', property='power', value=after), steps[split:])
            untap = max(i for i, s in enumerate(steps) if s['op'] == 'untap_step')
            self.assertIn(dict(op='assert_card', ref='host_0_0', property='tapped', value=int(name != 'Juntu Stakes')), steps[untap:])

    def test_manual_untap_and_counter_response_use_real_protocol(self):
        for owner in (0, 1):
            steps = self.case('Choke', 'manual_untap', owner)['steps']
            target = next(i for i, s in enumerate(steps) if s['op'] == 'choose_target')
            self.assertEqual([s['op'] for s in steps[target:target+5]], ['choose_target', 'cast', 'assert_stack', 'choose_dialog', 'resolve'])
            self.assertEqual(steps[target+3], dict(op='choose_dialog', player=owner, value=1))
            steps = self.case('Curse of Marit Lage', 'countered', owner)['steps']
            response = next(i for i, s in enumerate(steps) if s['op'] == 'cast_response')
            self.assertEqual(steps[response-1], dict(op='announce_spell', ref='source'))
            self.assertFalse(any(s['op'].startswith('choose_') for s in steps))

    def test_source_self_lock_follows_real_combat(self):
        steps = self.case('Marble Titan', 'source_attacks', 0)['steps']
        end = next(i for i, s in enumerate(steps) if s['op'] == 'end_combat')
        self.assertEqual([s['op'] for s in steps[end-2:end+1]], ['advance_combat', 'combat_damage', 'end_combat'])
        self.assertIn(dict(op='assert_life', player=1, value=17), steps[end:])
        untap = max(i for i, s in enumerate(steps) if s['op'] == 'untap_step')
        self.assertIn(dict(op='assert_card', ref='source', property='tapped', value=1), steps[untap:])

    def test_generated_cases_and_replay_provenance(self):
        count = 0
        for name in model.pins()['sources']:
            declaration = self.declaration(name)
            p = model.spec(declaration, self.oracle[name])
            cases = model.expand(name, p['csvid'], declaration, p, model.variants(p), self.rows, self.oracle)
            count += len(cases)
            for case in cases:
                self.assertEqual(case['_recipe']['covered_clauses'], self.oracle[name]['oracle_text'].splitlines())
                for action in case['steps']:
                    self.assertIn(action['op'], OPS)
                    if 'property' in action:
                        self.assertIn(action['property'], PROPERTIES)
                if 'replay' in case['name']:
                    steps = case['steps']
                    ops = [s['op'] for s in steps]
                    self.assertEqual(steps[ops.index('capture')+1:ops.index('record_state')], steps[ops.index('restore')+1:-1])
                    self.assertEqual(ops[-1], 'assert_recorded_state')
        self.assertEqual(count, 198)
