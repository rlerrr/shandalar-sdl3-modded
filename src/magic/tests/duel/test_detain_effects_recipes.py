import json
import unittest
from pathlib import Path

from detain_effects_recipes import FAMILY, classify, expand, profiles, spec, validate_witnesses, variants
from generate_cases import registry
from recipes import COLORS


class DetainEffectsRecipesTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle = json.loads((Path(__file__).parent / 'data/oracle.json').read_bytes())['cards']
        cls.registry = {row['name']: row for row in registry()}

    def case(self, card, scenario, owner):
        record = self.oracle[card]
        variant = f'{scenario}_{owner}'
        declaration = dict(family=FAMILY, version=2, oracle_text=record['oracle_text'],
                           params=classify(record), variants=[variant])
        case, = expand(card, self.registry[card]['csvid'], declaration, spec(declaration, record), [variant],
                       self.registry, self.oracle)
        return case

    def actions(self, case, op, **filters):
        return [action for action in case['steps'] if action['op'] == op
                and all(action.get(field) == value for field, value in filters.items())]

    def values(self, case, op, **filters):
        return [action['value'] for action in self.actions(case, op, **filters)]

    def last(self, case, op, **filters):
        return self.values(case, op, **filters)[-1]

    def test_complete_pinned_models(self):
        validate_witnesses(self.oracle, self.registry)
        sources = profiles()['sources']
        self.assertEqual({item['registry']['csvid'] for item in sources.values()},
                         {12660, 12663, 12750, 12752, 12769, 13169, 12830, 12771, 13166, 12650})
        self.assertEqual(sum(len(variants(classify(self.oracle[name]))) for name in sources), 240)
        for name in sources:
            row = self.oracle[name]
            with self.assertRaises(ValueError):
                classify(dict(row, oracle_text=row['oracle_text'].replace('Until your next turn', 'Until end of turn')))
            with self.assertRaises(ValueError):
                spec(dict(family=FAMILY, version=2, oracle_text=row['oracle_text'], variants=['effect_0'],
                          params=dict(classify(row), maximum=float(classify(row)['maximum']))), row)

    def test_real_etb_and_cast_timing_and_no_legal_targets(self):
        for owner in (0, 1):
            for name in ('Azorius Arrester', 'Inaction Injunction', 'Lyev Decree'):
                case = self.case(name, 'effect', owner)
                target, = self.actions(case, 'choose_target', ref='target', player=owner)
                cast, = self.actions(case, 'cast', ref='source', value=1)
                self.assertEqual(case['steps'].index(cast) < case['steps'].index(target), name == 'Azorius Arrester')
                no_target = self.case(name, 'no_target', owner)
                self.assertFalse(self.actions(no_target, 'choose_target'))
                self.assertEqual(self.values(no_target, 'cast', ref='source'), [int(name != 'Inaction Injunction')])
                for scene in ('short', 'wrong_color'):
                    short = self.case(name, scene, owner)
                    self.assertEqual(self.values(short, 'cast', ref='source'), [0])
                    self.assertFalse(self.actions(short, 'choose_target'))

    def test_both_mana_and_nonmana_activations_with_live_unaffected_controls(self):
        for owner in (0, 1):
            for name in ('Azorius Arrester', 'Lyev Decree'):
                lock = self.case(name, 'activation_lock', owner)
                self.assertEqual(self.values(lock, 'activate', ref='target'), [0])
                self.assertEqual(self.values(lock, 'activate', ref='friendly'), [1])
                self.assertEqual(self.last(lock, 'assert_life', player=owner), 20)
                self.assertEqual(self.last(lock, 'assert_life', player=1-owner), 19)
                mana = self.case(name, 'mana_lock', owner)
                self.assertEqual(self.values(mana, 'activate', ref='target'), [0])
                self.assertEqual(self.values(mana, 'activate', ref='friendly'), [1])
                self.assertEqual(self.last(mana, 'assert_mana', player=owner, color=COLORS['G']), 1)
                self.assertEqual(self.last(mana, 'assert_mana', player=1-owner, color=COLORS['G']), 0)

    def test_zero_one_and_two_preserve_real_optional_policy(self):
        for owner in (0, 1):
            for name in ('Azorius Justiciar', 'Lyev Decree'):
                for scenario, selected, optional, effects in (('zero', [], ['target'], [1]),
                                                              ('stop_after_one', ['target'], ['second'], [0, 1]),
                                                              ('two', ['target', 'second'], [], [0, 0])):
                    case = self.case(name, scenario, owner)
                    self.assertEqual([step['ref'] for step in self.actions(case, 'choose_target')], selected)
                    self.assertEqual([step['ref'] for step in self.actions(case, 'choose_optional_target')], optional)
                    self.assertEqual(self.values(case, 'assert_forced_target_choices'), [0] if optional else [])
                    self.assertEqual(self.values(case, 'activate'), effects)

    def test_turn_boundary_and_source_removal_are_real(self):
        for owner in (0, 1):
            expiry = self.case('Azorius Arrester', 'expiry', owner)
            self.assertEqual(self.values(expiry, 'block', ref='target'), [0, 1])
            self.assertEqual(self.values(expiry, 'attack', ref='target'), [0])
            self.assertEqual([step['player'] for step in self.actions(expiry, 'untap_step')], [1-owner, owner])
            self.assertEqual(self.values(expiry, 'can_activate', ref='target'), [1])
            gone = self.case('Azorius Arrester', 'source_removed', owner)
            self.assertEqual(self.values(gone, 'cast', ref='reply'), [1])
            self.assertTrue(self.actions(gone, 'choose_target', ref='source', player=1-owner))
            self.assertEqual(self.last(gone, 'assert_zone_multiplicity', player=owner, zone='hand', index=12660), 1)
            self.assertEqual(self.values(gone, 'activate', ref='target'), [0])

    def test_targets_lost_and_exact_draw(self):
        for owner in (0, 1):
            for scenario, drawn in (('effect', 1), ('target_lost', 0), ('countered', 0), ('no_target', 0), ('one_card', 1)):
                case = self.case('Inaction Injunction', scenario, owner)
                self.assertEqual(self.last(case, 'assert_zone_multiplicity', player=owner, zone='hand',
                                           index=self.registry['Mountain']['csvid']), drawn)
                if scenario in ('target_lost', 'countered'):
                    self.assertEqual(self.values(case, 'cast_response', ref='reply'), [1])
            partial = self.case('Lyev Decree', 'partial_lost', owner)
            self.assertEqual(self.values(partial, 'cast_response', ref='reply'), [1])
            self.assertEqual(self.values(partial, 'activate', ref='second'), [0])
            self.assertFalse(self.actions(partial, 'activate', ref='target'))

    def test_nonland_includes_artifact_abilities_but_does_not_detain_land(self):
        for owner in (0, 1):
            for scenario in ('artifact_mana', 'artifact_nonmana'):
                case = self.case('Lyev Skyknight', scenario, owner)
                self.assertTrue(self.actions(case, 'choose_target', ref='target'))
                self.assertEqual(self.values(case, 'activate', ref='target'), [0])
                self.assertEqual(self.values(case, 'activate', ref='land'), [1])
                self.assertEqual(self.last(case, 'assert_card', ref='target', property='tapped'), 0)
                self.assertEqual(self.last(case, 'assert_mana', player=1-owner, color=COLORS['G']), 1)

    def test_flying_is_observed_with_real_aged_source_and_replay_holds(self):
        for owner in (0, 1):
            for name in ("Isperia's Skywatch", 'Lyev Skyknight'):
                for scenario, accepted in (('flying_ground', 0), ('flying_reach', 1)):
                    case = self.case(name, scenario, owner)
                    self.assertEqual(self.values(case, 'attack', ref='source'), [1])
                    self.assertEqual(self.values(case, 'block', other='source'), [accepted])
                    self.assertEqual([step['player'] for step in self.actions(case, 'untap_step')], [1-owner, owner])
            replay = self.case('Azorius Justiciar', 'replay', owner)
            self.assertEqual(self.values(replay, 'cast', ref='source'), [1, 1])
            self.assertTrue(self.actions(replay, 'assert_recorded_state'))

    def test_spirit_denies_real_block_after_its_detain_has_expired(self):
        for owner in (0, 1):
            spirit = self.case('Soulsworn Spirit', 'unblockable', owner)
            self.assertEqual(self.values(spirit, 'cast', ref='source'), [1])
            self.assertEqual(self.values(spirit, 'attack', ref='source'), [1])
            self.assertEqual(self.values(spirit, 'block', ref='blocker'), [0])
            self.assertEqual(len(self.actions(spirit, 'untap_step')), 2)

    def test_martial_law_alternates_only_at_real_own_upkeeps(self):
        for owner in (0, 1):
            case = self.case('Martial Law', 'alternate_targets', owner)
            self.assertEqual([step['player'] for step in self.actions(case, 'upkeep_step')], [1-owner, owner, 1-owner, owner])
            self.assertEqual([step['ref'] for step in self.actions(case, 'choose_target')], ['target', 'second'])
            self.assertEqual(self.values(case, 'attack', ref='target'), [0])
            self.assertEqual(self.values(case, 'activate', ref='target'), [1])
            self.assertEqual(self.values(case, 'activate', ref='second'), [0])

    def test_lavinia_threshold_and_red_protection_have_independent_witnesses(self):
        for owner in (0, 1):
            mass = self.case('Lavinia of the Tenth', 'mass', owner)
            self.assertFalse(self.actions(mass, 'choose_target'))
            self.assertEqual(self.values(mass, 'block', ref='protected'), [0])
            self.assertEqual(self.values(mass, 'activate', ref='target'), [0])
            self.assertEqual(self.values(mass, 'activate', ref='over'), [1])
            cutoff = self.case('Lavinia of the Tenth', 'artifact_cutoff', owner)
            self.assertEqual(self.values(cutoff, 'activate', ref='cutoff'), [0])
            protect = self.case('Lavinia of the Tenth', 'protection_target', owner)
            self.assertEqual(self.values(protect, 'can_target_creature', ref='red_probe'), [0])
            self.assertEqual(self.values(protect, 'can_target_creature', ref='blue_probe'), [1])
            burn = self.case('Lavinia of the Tenth', 'protection_damage', owner)
            self.assertEqual(self.values(burn, 'cast', ref='burn'), [1])
            self.assertEqual(self.last(burn, 'assert_card', ref='source', property='damage'), 0)
            self.assertEqual(self.last(burn, 'assert_zone_multiplicity', player=owner, zone='graveyard', index=106), 1)

    def test_archon_targets_zero_one_and_two_during_actual_attack(self):
        for owner in (0, 1):
            for name, selected, allowed in (('zero', [], [1, 1]), ('attack_one', ['target'], [0, 1]),
                                            ('attack_two', ['target', 'artifact'], [0, 0])):
                case = self.case('Archon of the Triumvirate', name, owner)
                self.assertEqual([step['ref'] for step in self.actions(case, 'choose_target')], selected)
                self.assertEqual(self.values(case, 'attack', ref='source'), [1])
                self.assertEqual(self.values(case, 'activate'), allowed)
            fresh = self.case('Archon of the Triumvirate', 'fresh', owner)
            self.assertEqual(self.values(fresh, 'attack', ref='source'), [0])
            self.assertEqual(self.values(fresh, 'activate', ref='target'), [1])
