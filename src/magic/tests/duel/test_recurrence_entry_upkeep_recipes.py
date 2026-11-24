"""Regression guards for distinct entry, recurrence, and upkeep boundaries."""
import copy
import json
from pathlib import Path
import unittest

from generate_cases import registry
from recipes import expand
import recurrence_entry_upkeep_recipes as family


class RecurrenceEntryUpkeepTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        base = Path(__file__).parent
        cls.oracle = json.loads((base/'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.registry = {row['name']: row for row in registry()}
        cls.cases, cls.declarations = {}, {}
        for name, pin in family.pins()['sources'].items():
            csvid = pin['registry']['csvid']
            path = next((base/'data/cases/cards'/f'{csvid//100:03d}').glob(f'{csvid:05d}-*.json'))
            declaration = json.loads(path.read_text(encoding='utf8'))['recipes'][0]
            cls.declarations[name] = declaration
            cls.cases[name] = {c['_recipe']['variant']: c for c in expand(
                name, csvid, declaration, cls.oracle[name], cls.registry, cls.oracle)}

    def steps(self, name, variant):
        return self.cases[name][variant]['steps']

    def test_whole_oracle_and_identity_mutations_fail_closed(self):
        self.assertEqual(sum(map(len, self.cases.values())), 110)
        for name, declaration in self.declarations.items():
            record = self.oracle[name]
            family.spec(declaration, record)
            for key, value in [('version', 2.0), ('family', 'death_recurrence'), ('variants', ['cycle_0', 'cycle_0'])]:
                with self.assertRaises(ValueError):
                    family.spec(dict(declaration, **{key: value}), record)
            for text in [record['oracle_text']+'\nFlying', record['oracle_text'].replace("under its owner's control", 'under your control')]:
                with self.assertRaises(ValueError):
                    family.oracle_model(dict(record, oracle_text=text))
            altered = copy.deepcopy(declaration)
            altered['params']['effect']['kind'] = 'draw'
            with self.assertRaises(ValueError):
                family.spec(altered, record)
        registry_pin = copy.deepcopy(self.registry)
        registry_pin['Murder']['handler'] = 'card_dummy'
        with self.assertRaises(ValueError):
            family.validate_witnesses(self.oracle, registry_pin)

    def test_real_deaths_return_once_under_owner_with_natural_counter(self):
        for name in self.cases:
            for owner in (0, 1):
                steps = self.steps(name, f'cycle_{owner}')
                csvid = self.registry[name]['csvid']
                binds = [s for s in steps if s['op'] == 'bind_permanent']
                self.assertEqual(binds, [dict(op='bind_permanent', ref='returned_1', player=owner, value=csvid)])
                first = steps.index(binds[0])
                self.assertIn(dict(op='cast', ref='death_1', value=1), steps[:first])
                self.assertIn(dict(op='cast', ref='death_2', value=1), steps[first:])
                counter = 'plus1' if 'Undying' in self.oracle[name]['keywords'] else 'minus1'
                self.assertIn(dict(op='assert_counter', ref='returned_1', counter_kind=counter, value=1), steps[first:])
                self.assertIn(dict(op='assert_card', ref='returned_1', property='in_play', value=0), steps[first:])

    def test_messenger_entry_and_return_are_tapped_and_sole_target_is_automatic(self):
        name = "Geralf's Messenger"
        for owner in (0, 1):
            steps = self.steps(name, f'cycle_{owner}')
            for ref in ('source', 'returned_1'):
                self.assertIn(dict(op='assert_card', ref=ref, property='tapped', value=1), steps)
            for life in (18, 16):
                self.assertIn(dict(op='assert_life', player=1-owner, value=life), steps)
            self.assertFalse(any(s['op'] == 'choose_player' for s in steps))
            protected = self.steps(name, f'opponent_shroud_{owner}')
            self.assertEqual({s['value'] for s in protected if s['op'] == 'assert_life'}, {20})

    def test_mindcrusher_selects_only_actual_entry_windows_and_preserves_library_order(self):
        name = "Geralf's Mindcrusher"
        steps = self.steps(name, 'cycle_0')
        choices = [i for i, s in enumerate(steps) if s['op'] == 'choose_player']
        self.assertEqual(len(choices), 2)
        for i in choices:
            self.assertEqual(steps[i], dict(op='choose_player', player=0, value=1))
            self.assertEqual(steps[i+1], dict(op='resolve'))
        for i, card in enumerate(family.LIBRARY[10:]):
            self.assertIn(dict(op='assert_zone_card', player=1, zone='library', index=i,
                               value=self.registry[card]['csvid']), steps)
        short = self.steps(name, 'short_library_1')
        self.assertIn(dict(op='assert_zone', player=0, zone='graveyard', value=4), short)
        self.assertIn(dict(op='assert_zone', player=0, zone='library', value=0), short)
        self.assertIn(dict(op='choose_player', player=0, value=0), self.steps(name, 'self_target_0'))

    def test_redcap_targets_each_permitted_class_and_uses_returned_power(self):
        name = 'Murderous Redcap'
        for owner in (0, 1):
            for mode, op, key, wanted in [('cycle', 'assert_life', 'player', 1-owner),
                                           ('creature_target', 'assert_card', 'ref', 'damage_target'),
                                           ('planeswalker_target', 'assert_counter', 'ref', 'damage_target')]:
                steps = self.steps(name, f'{mode}_{owner}')
                self.assertIn(dict(op='assert_card', ref='returned_1', property='power', value=1), steps)
                self.assertTrue(any(s['op'] == op and s.get(key) == wanted for s in steps))
            self.assertIn(dict(op='assert_life', player=1-owner, value=17), self.steps(name, f'cycle_{owner}'))
            self.assertIn(dict(op='assert_card', ref='damage_target', property='damage', value=3),
                          self.steps(name, f'creature_target_{owner}'))
            walker = self.steps(name, f'planeswalker_target_{owner}')
            self.assertIn(dict(op='assert_counter', ref='damage_target', counter_kind='loyalty', value=1), walker)
            self.assertIn(dict(op='assert_zone_multiplicity', player=1-owner, zone='graveyard', index=9185, value=1), walker)

    def test_heartmender_real_upkeeps_scope_one_counter_per_controlled_creature(self):
        for owner in (0, 1):
            steps = self.steps('Heartmender', f'upkeep_scope_{owner}')
            upkeeps = [i for i, s in enumerate(steps) if s['op'] == 'upkeep_step']
            self.assertEqual([steps[i]['player'] for i in upkeeps], [1-owner, owner])
            for i, opponent_upkeep in zip(upkeeps, (True, False)):
                self.assertEqual(steps[i-1]['op'], 'untap_step')
                after = steps[i:upkeeps[1]] if opponent_upkeep else steps[i:]
                for ref, counter in [('ally_counter', 2 if opponent_upkeep else 1), ('enemy_counter', 1),
                                     ('returned_1', int(opponent_upkeep)), ('ally_plain', 0)]:
                    self.assertIn(dict(op='assert_counter', ref=ref, counter_kind='minus1', value=counter), after)
            self.assertEqual([s['ref'] for s in steps if s['op'] == 'bind_permanent'], ['returned_1', 'returned_2'])
            self.assertEqual(sum(s['op']=='activate' and s['ref'].startswith('spitter_') for s in steps), 3)

    def test_countered_zero_cost_and_bounce_never_enqueue_entry_choices(self):
        for name in self.cases:
            for owner in (0, 1):
                for mode in ('countered', 'zero_mana'):
                    steps = self.steps(name, f'{mode}_{owner}')
                    self.assertFalse(any(s['op'] in ('choose_player', 'choose_target', 'bind_permanent') for s in steps))
                bounce = self.steps(name, f'bounce_{owner}')
                self.assertFalse(any(s['op'] == 'bind_permanent' for s in bounce))

    def test_replay_boundaries_have_actual_pending_or_upkeep_actions(self):
        for name in self.cases:
            for mode in ('cast_replay', 'death_replay'):
                steps = self.steps(name, mode+'_0')
                boundary = next(i for i, s in enumerate(steps) if s['op'] == 'capture')
                self.assertIn(steps[boundary+1]['op'], ('resolve', 'choose_player'))
                self.assertIn(dict(op='restore'), steps)
                self.assertEqual(steps[-1], dict(op='assert_recorded_state'))
        steps = self.steps('Heartmender', 'upkeep_replay_1')
        capture = steps.index(dict(op='capture'))
        self.assertEqual(steps[capture-1:capture+2], [dict(op='untap_step', player=1), dict(op='capture'),
                                                       dict(op='upkeep_step', player=1)])


if __name__ == '__main__':
    unittest.main()
