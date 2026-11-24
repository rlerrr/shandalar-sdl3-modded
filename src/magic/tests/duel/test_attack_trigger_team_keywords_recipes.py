"""Focused timing and scope guards for attack-trigger recipes."""
from copy import deepcopy
import unittest

import attack_trigger_team_keywords_recipes as model


class AttackTeamKeywordTests(unittest.TestCase):
    def setUp(self):
        pins = model.profiles()
        entries = pins['sources'] | pins['witnesses']
        self.oracle = {name: p['oracle'] for name, p in entries.items()}
        self.rows = {name: p['registry'] for name, p in entries.items()}
        self.ids = {r['csvid']: r for r in self.rows.values()}

    def run_mode(self, key, mode, seat):
        game = model.FollowBoard(self.ids[key], mode, seat, self.rows, self.oracle)
        game.run()
        return game

    def test_full_oracle_and_identity_pins(self):
        model.validate_witnesses(self.oracle, self.rows)
        record = deepcopy(self.oracle['Veteran\'s Armaments'])
        record['oracle_text'] = record['oracle_text'].replace('Soldier creature', 'creature')
        with self.assertRaises(ValueError):
            model.classify(record, model.ARMAMENTS)

    def test_distinct_flight_target_scope(self):
        for seat in (0, 1):
            for csvid, mode, target in ((model.DRAKE, 'self_flight', 'source'),
                                        (model.CONDOR, 'no_other', None),
                                        (model.CONDOR, 'resting', 'partner')):
                game = self.run_mode(csvid, mode, seat)
                self.assertEqual([s['ref'] for s in game.steps if s['op'] == 'choose_target'],
                                 [] if target is None else [target])

    def test_foreign_umbra_untaps_host_side_lands(self):
        game = self.run_mode(model.UMBRA, 'foreign_host', 0)
        self.assertEqual(game.life, [16, 20])
        assertions = {s['ref']: s['value'] for s in game.steps if s['op'] == 'assert_card' and s['property'] == 'tapped'}
        self.assertEqual((assertions['own_land'], assertions['foe_land']), (1, 0))

    def test_actual_fabricate_receipts_and_distinct_decline(self):
        game = self.run_mode(model.BLADES, 'servo_entry', 0)
        self.assertEqual(len([s for s in game.steps if s['op'] == 'bind_permanent_copy']), 2)
        self.assertEqual(len([s for s in game.steps if s['op'] == 'choose_trigger_source']), 1)
        self.assertFalse(any(s['op'] == 'choose_trigger_source' for s in self.run_mode(model.BLADES, 'decline', 0).steps))
        self.assertEqual(model.variants({'csvid': model.BLADES}).count('decline_0'), 1)
        self.assertNotIn('decline_1', model.variants({'csvid': model.BLADES}))

    def test_outrider_threshold_is_true_at_declaration(self):
        for seat in (0, 1):
            for mode, power in (('below', 7), ('exactly', 8), ('late_threshold', 7)):
                game = self.run_mode(model.OUTRIDER, mode, seat)
                initial_team = [item['name'] for item in game.setup if item['player'] == seat
                                and (item['zone'] == 'battlefield' or item['ref'] == 'source')]
                self.assertEqual(sum(int(self.oracle[name]['power']) for name in initial_team), power)
                self.assertFalse(any(item['ref'] == 'partner' for item in game.setup))
