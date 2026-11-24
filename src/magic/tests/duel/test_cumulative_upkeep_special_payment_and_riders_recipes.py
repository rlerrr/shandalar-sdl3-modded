"""Focused recipe guards for real special cumulative upkeep actions."""
from copy import deepcopy
import unittest

from cumulative_upkeep_special_payment_and_riders_recipes import (
    ANCIENT, FAMILY, LASH, MINOTAUR, WITNESSES, Board, classify, profiles, spec,
    validate_variants, validate_witnesses, variants,
)


def inputs():
    profile = profiles()
    records = {name: pin['oracle'] for group in ('sources', 'witnesses')
               for name, pin in profile[group].items()}
    registered = {name: pin['registry'] for group in ('sources', 'witnesses')
                  for name, pin in profile[group].items()}
    registered.update(profile['token_registry'])
    return records, registered


def assemble(csvid, mode, actor):
    oracle, registered = inputs()
    name = next(name for name in profiles()['sources'] if registered[name]['csvid'] == csvid)
    board = Board(oracle[name], csvid, actor, mode, registered, oracle)
    board.build()
    return board.steps


class SpecialUpkeepGuards(unittest.TestCase):
    def test_exact_sources_witnesses_and_registry(self):
        oracle, registered = inputs()
        validate_witnesses(oracle, registered)
        for name in WITNESSES:
            changed = deepcopy(registered)
            changed[name]['iid'] += 1
            with self.assertRaises(ValueError):
                validate_witnesses(oracle, changed)

    def test_whole_oracle_including_nonpayment_and_prevention(self):
        record = deepcopy(profiles()['sources']['Thought Lash']['oracle'])
        model = classify(record, LASH)
        declaration = dict(family=FAMILY, version=2, params=model, oracle_text=record['oracle_text'],
                           variants=variants(model))
        self.assertEqual(spec(declaration, record)['csvid'], LASH)
        for line in record['oracle_text'].splitlines():
            changed = dict(record, oracle_text=record['oracle_text'].replace(line, 'omitted'))
            with self.assertRaises(ValueError):
                spec(declaration, changed)

    def test_schema_fails_closed(self):
        record = profiles()['sources']['Sheltering Ancient']['oracle']
        model = classify(record, ANCIENT)
        declaration = dict(family=FAMILY, version=2, params=model, oracle_text=record['oracle_text'],
                           variants=variants(model))
        for changed in (dict(declaration, version=2.0), dict(declaration, family='other'),
                        dict(declaration, params=dict(model, csvid=float(ANCIENT)))):
            with self.assertRaises(ValueError):
                spec(changed, record)
        for labels in (['trample_0', 'trample_0'], ['trample_2'], []):
            with self.assertRaises(ValueError):
                validate_variants(model, labels)

    def test_ancient_repeats_real_opponent_recipient(self):
        for actor in (0, 1):
            actions = assemble(ANCIENT, 'repeat_recipient', actor)
            self.assertEqual([s['player'] for s in actions if s['op'] == 'upkeep_step'],
                             [1 - actor, actor, 1 - actor, actor])
            self.assertEqual([(s['ref'], s['player']) for s in actions if s['op'] == 'choose_target'],
                             [('foe_bear', actor)] * 3)
            self.assertIn(3, [s['value'] for s in actions if s['op'] == 'assert_card'
                              and s.get('ref') == 'foe_bear' and s.get('property') == 'plus1_counters'])

    def test_ancient_unpayable_does_not_offer_own_creature(self):
        for actor in (0, 1):
            actions = assemble(ANCIENT, 'no_opposing_creature', actor)
            self.assertFalse(any(s['op'] == 'choose_target' for s in actions))
            self.assertTrue(any(s['op'] == 'assert_zone_multiplicity' and s['index'] == ANCIENT
                                and s['zone'] == 'graveyard' and s['value'] == 1 for s in actions))

    def test_minotaur_calls_both_faces_and_scales_real_damage(self):
        for actor in (0, 1):
            for variant, face in (('heads', 0), ('tails', 1)):
                actions = assemble(MINOTAUR, variant, actor)
                self.assertEqual([(s['player'], s['value']) for s in actions if s['op'] == 'choose_dialog'],
                                 [(actor, 0), (actor, face)])
                self.assertEqual([(s['player'], s['ref']) for s in actions if s['op'] == 'choose_target'],
                                 [(-1, 'foe_bear')])
            actions = assemble(MINOTAUR, 'scaled', actor)
            self.assertEqual([s['value'] for s in actions if s['op'] == 'choose_player'],
                             [1 - actor, 1 - actor, actor])
            self.assertEqual([s['value'] for s in actions if s['op'] == 'assert_life' and s['player'] == actor][-1], 19)
            self.assertEqual([s['value'] for s in actions if s['op'] == 'assert_life' and s['player'] == 1 - actor][-1], 18)

