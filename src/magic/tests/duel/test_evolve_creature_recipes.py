import json
import unittest
from pathlib import Path

from evolve_creature_recipes import FAMILY, classify, expand, profiles, spec, validate_witnesses, variants
from generate_cases import registry


class EvolveCreatureRecipesTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle = json.loads((Path(__file__).parent / 'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {row['name']: row for row in registry()}

    def declaration(self, name, selected):
        record = self.oracle[name]
        return dict(family=FAMILY, version=2, params=classify(record), oracle_text=record['oracle_text'], variants=selected)

    def case(self, name, mode, owner=0):
        selected = [f'{mode}_{owner}']
        declaration = self.declaration(name, selected)
        params = spec(declaration, self.oracle[name])
        case, = expand(name, self.rows[name]['csvid'], declaration, params, selected, self.rows, self.oracle)
        self.assertTrue(all(not card.get('state') for card in case['setup']))
        return case

    def actions(self, case, op, **fields):
        return [s for s in case['steps'] if s['op'] == op and all(s.get(key) == value for key, value in fields.items())]

    def receipts(self, case, ref='source', prop='plus1_counters'):
        return [s['value'] for s in self.actions(case, 'assert_card', ref=ref, property=prop)]

    def test_whole_oracle_binding_and_unmodeled_companion_fail_closed(self):
        validate_witnesses(self.oracle, self.rows)
        expected = {12875, 13119, 12908, 12909, 12915, 13052, 12937, 13205}
        self.assertEqual({self.rows[n]['csvid'] for n in profiles()['sources']}, expected)
        for name in profiles()['sources']:
            params = classify(self.oracle[name])
            declaration = self.declaration(name, variants(params))
            self.assertEqual(spec(declaration, self.oracle[name])['keyword'], params['keyword'])
            with self.assertRaises(ValueError):
                spec(dict(declaration, version=2.0), self.oracle[name])
            with self.assertRaises(ValueError):
                spec(dict(declaration, params=dict(params, power=float(params['power']))), self.oracle[name])
        for name in ('Fathom Mage', 'Simic Manipulator'):
            with self.assertRaises(ValueError):
                classify(self.oracle[name])

    def test_power_toughness_both_and_equal_themes_have_real_entrants(self):
        for name in profiles()['sources']:
            for mode in ('power_only', 'toughness_only', 'both_greater', 'equal'):
                c = self.case(name, mode)
                self.assertEqual(self.actions(c, 'cast', ref='first')[0]['value'], 1)
                self.assertEqual(self.receipts(c), [0, int(mode != 'equal')])
                self.assertEqual(self.receipts(c, ref='first'), [0])

    def test_noncreature_enemy_and_weaker_entries_remain_no_growth(self):
        for mode in ('noncreature_entry', 'opponent_entry', 'weaker'):
            c = self.case('Adaptive Snapjaw', mode, 1)
            self.assertEqual(self.receipts(c)[-1], 0)
            self.assertEqual(self.actions(c, 'cast', ref='first')[0]['value'], 1)
        enemy = self.case('Adaptive Snapjaw', 'opponent_entry', 1)
        entrant, = [card for card in enemy['setup'] if card['ref'] == 'first']
        self.assertEqual(entrant['player'], 0)
        self.assertEqual(self.actions(enemy, 'untap_step')[0]['player'], 0)

    def test_paid_entries_use_printed_cost_and_rejected_cast_retains_hand(self):
        c = self.case('Adaptive Snapjaw', 'power_only', 1)
        first_source = self.actions(c, 'cast', ref='source')[0]
        batches = self.actions(c, 'set_mana', player=1)
        self.assertEqual([s['value'] for s in batches[6:12]], [4, 0, 0, 1, 0, 0])
        self.assertEqual(first_source['value'], 1)
        for mode in ('insufficient_mana', 'wrong_color'):
            denied = self.case('Clinging Anemones', mode)
            self.assertEqual(self.actions(denied, 'cast', ref='source')[0]['value'], 0)
            self.assertEqual(self.receipts(denied, prop='in_hand'), [1])
            self.assertFalse(self.actions(denied, 'resolve'))

    def test_current_stats_and_each_watcher_eligibility_are_independent(self):
        c = self.case('Crocanura', 'sequence')
        self.assertEqual(self.receipts(c), [0, 1, 1, 2])
        c = self.case('Cloudfin Raptor', 'lower_after_growth', 1)
        self.assertEqual(self.receipts(c), [0, 1, 1])
        c = self.case('Clinging Anemones', 'separate_sources')
        self.assertEqual(self.receipts(c), [0, 1, 1, 1])
        self.assertEqual(self.receipts(c, ref='second'), [0, 1])

    def test_real_piper_and_bounce_reset_exercise_entry_and_new_object(self):
        c = self.case('Battering Krasis', 'piper_entry')
        self.assertTrue(self.actions(c, 'activate', ref='piper'))
        self.assertTrue(self.actions(c, 'choose_zone_card'))
        self.assertFalse(self.actions(c, 'cast', ref='first'))
        self.assertEqual(self.receipts(c), [0, 1])
        c = self.case('Clinging Anemones', 'bounce_reset', 1)
        self.assertEqual(len(self.actions(c, 'cast', ref='source')), 2)
        self.assertEqual(self.receipts(c), [0, 1, 0, 1])
        self.assertTrue(self.actions(c, 'bind_hand_copy', ref='source'))

    def test_real_pre_entry_response_and_countered_entrant(self):
        for owner in (0, 1):
            c = self.case('Adaptive Snapjaw', 'response_prevents', owner)
            ops = [s['op'] for s in c['steps']]
            ann, cast, drain = map(ops.index, ('announce_spell', 'cast_response', 'resolve_response'))
            self.assertLess(ann, cast)
            self.assertLess(cast, drain)
            self.assertEqual(self.receipts(c, prop='power'), [6, 9, 6])
            self.assertEqual(self.receipts(c), [0, 0, 0])
            self.assertEqual(self.receipts(c, ref='first', prop='power'), [7, 7])
            c = self.case('Battering Krasis', 'countered_entrant', owner)
            self.assertTrue(self.actions(c, 'assert_stack', value=2))
            self.assertEqual(self.receipts(c), [0, 0])
            self.assertEqual(self.receipts(c, ref='first'), [])
            self.assertTrue(self.actions(c, 'assert_zone_multiplicity', player=owner, zone='graveyard', index=110, value=1))

    def test_real_printed_combat_rules(self):
        for owner in (0, 1):
            trample = self.case('Battering Krasis', 'trample_combat', owner)
            self.assertTrue(self.actions(trample, 'block', ref='opposing', other='source', value=1))
            self.assertTrue(self.actions(trample, 'assert_life', player=1-owner, value=18))
            flying = self.case('Cloudfin Raptor', 'flying_combat', owner)
            self.assertTrue(self.actions(flying, 'block', ref='opposing', other='source', value=0))
            self.assertTrue(self.actions(flying, 'assert_life', player=1-owner, value=19))
            reach = self.case('Crocanura', 'reach_combat', owner)
            self.assertTrue(self.actions(reach, 'block', ref='source', other='opposing', value=1))
            self.assertTrue(self.actions(reach, 'assert_zone_multiplicity', player=1-owner, zone='graveyard', index=1162, value=1))
            defender = self.case('Clinging Anemones', 'defender_combat', owner)
            self.assertEqual(len(self.actions(defender, 'untap_step')), 2)
            self.assertTrue(self.actions(defender, 'attack', ref='source', value=0))
            self.assertEqual(self.receipts(defender, prop='sick'), [0])
            defender_ops = [step['op'] for step in defender['steps']]
            done = defender_ops.index('finish_attackers')
            self.assertEqual(defender_ops[done:done+6], [
                'finish_attackers', 'begin_blockers', 'advance_combat', 'combat_damage', 'resolve', 'end_combat',
            ])

    def test_snapshot_replays_actual_pending_entry(self):
        c = self.case('Adaptive Snapjaw', 'pending_replay', 1)
        steps = c['steps']
        ops = [s['op'] for s in steps]
        capture, record, restore = map(ops.index, ('capture', 'record_state', 'restore'))
        self.assertEqual(steps[capture+1:record], steps[restore+1:-1])
        self.assertEqual(ops[capture+1], 'resolve')
        self.assertEqual(self.receipts(c), [0, 1, 1])

    def test_flash_and_unblockable_compositions_use_real_actions(self):
        for owner in (0, 1):
            flash = self.case('Shambleshark', 'flash_response', owner)
            steps = flash['steps']
            names = [step['op'] for step in steps]
            turn, announcement, response = map(names.index, ('advance_turn', 'announce_spell', 'cast_response'))
            self.assertLess(turn, announcement)
            self.assertLess(announcement, response)
            self.assertTrue(self.actions(flash, 'cast_response', ref='source', value=1))
            self.assertTrue(self.actions(flash, 'assert_stack', value=2))
            self.assertTrue(self.actions(flash, 'assert_life', player=1-owner, value=25))
            self.assertEqual(self.receipts(flash), [0, 1])
            combat = self.case('Shambleshark', 'flash_block', owner)
            names = [step['op'] for step in combat['steps']]
            self.assertLess(names.index('begin_blockers'), names.index('cast'))
            self.assertLess(names.index('cast'), names.index('block'))
            self.assertTrue(self.actions(combat, 'block', ref='source', other='opposing', value=1))
            self.assertTrue(self.actions(combat, 'assert_zone_multiplicity',
                                         player=owner, zone='graveyard', index=13052, value=1))
            krasis = self.case('Elusive Krasis', 'unblockable_combat', owner)
            self.assertTrue(self.actions(krasis, 'block', ref='opposing', other='source', value=0))
            self.assertTrue(self.actions(krasis, 'assert_life', player=1-owner, value=19))

    def test_only_real_evolution_repeats_counters_on_countered_friendly_creatures(self):
        for owner in (0, 1):
            team = self.case('Renegade Krasis', 'team_counter', owner)
            external = self.case('Renegade Krasis', 'external_counter_no_team', owner)
            self.assertEqual(self.receipts(team), [0, 0, 0, 1])
            self.assertEqual(self.receipts(external), [0, 0, 1])
            self.assertEqual(self.receipts(team, 'ally')[-1], 2)
            self.assertEqual(self.receipts(team, 'enemy')[-1], 1)
            self.assertEqual(self.receipts(team, 'counterless')[-1], 0)
            self.assertEqual(self.receipts(external, 'ally')[-1], 1)
            self.assertEqual([s['ref'] for s in self.actions(team, 'cast')][:3], ['source', 'ally_burst', 'other_burst'])
