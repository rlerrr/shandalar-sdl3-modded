"""Focused guards for Scry triggers and their full printed riders."""

from copy import deepcopy
import unittest

import library_scry_triggers_recipes as model


class ScryTriggerTests(unittest.TestCase):
    def setUp(self):
        pins = model.profiles()
        self.oracle = {name: entry['oracle'] for name, entry in (pins['sources'] | pins['witnesses']).items()}
        self.rows = {name: entry['registry'] for name, entry in (pins['sources'] | pins['witnesses']).items()}

    def case(self, name, mode, seat):
        game = model.FollowOn(self.rows[name]['csvid'], self.oracle[name], seat, mode, self.rows, self.oracle)
        game.build()
        return game

    def test_exact_printed_sources_and_variants_fail_closed(self):
        model.validate_witnesses(self.oracle, self.rows)
        oracle = deepcopy(self.oracle)
        oracle['The Scarab God']['oracle_text'] = oracle['The Scarab God']['oracle_text'].replace('4/4', '3/3')
        with self.assertRaises(ValueError):
            model.validate_witnesses(oracle, self.rows)
        shape = model.classify(self.oracle['Prognostic Sphinx'], model.SPHINX)
        self.assertIn('top_reorder_0', model.variants(shape))
        self.assertNotIn('top_reorder_1', model.variants(shape))

    def test_kingpin_reacts_only_to_own_artifact_entry(self):
        for seat in (0, 1):
            own = self.case('Contraband Kingpin', 'own_artifact', seat)
            foe = self.case('Contraband Kingpin', 'foe_artifact', seat)
            self.assertEqual(own.library[seat][-1], model.ARTIFACT)
            self.assertEqual(foe.library[seat][0], model.ARTIFACT)
            self.assertIn(dict(op='assert_unused_choices', value=1), foe.steps)

    def test_opposing_opening_reveal_targets_its_owners_scry(self):
        game = self.case('Sphinx of Foresight', 'opening_reveal', 1)
        self.assertEqual(game.library[0], ['Island', model.DRAW])
        self.assertEqual(game.library[1][-1], model.ARTIFACT)
        upkeep = [step['player'] for step in game.steps if step['op'] == 'upkeep_step']
        self.assertEqual(upkeep, [0, 1])
        self.assertIn(dict(op='assert_unused_choices', value=2), game.steps)

    def test_scarab_copy_and_return_keep_owner_zone(self):
        for seat in (0, 1):
            copying = self.case('The Scarab God', 'copy_foe', seat)
            self.assertEqual(copying.cards['victim']['player'], 1 - seat)
            self.assertEqual(copying.cards['victim']['zone'], 'exile')
            self.assertEqual(copying.cards['copy']['player'], seat)
            self.assertIn(dict(op='assert_matching_permanents', property='subtype_Zombie', player=seat,
                               index=self.rows[model.HOST]['csvid'], value=1), copying.steps)
            for subtype in ('Human', 'Wizard'):
                self.assertIn(dict(op='assert_matching_permanents', property=f'subtype_{subtype}', player=seat,
                                   index=self.rows[model.HOST]['csvid'], value=0), copying.steps)
            dying = self.case('The Scarab God', 'dies', seat)
            self.assertEqual(dying.cards['source']['zone'], 'hand')
            self.assertTrue(any(step['op'] == 'advance_turn' for step in dying.steps))
