"""Focused guards for history witnesses, each-end-step scope, and full clauses."""

import json
import unittest

from generate_cases import registry
import each_end_step_history_trigger_recipes as model


class EachEndStepHistoryTriggerTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle = json.loads((model.PROFILE_PATH.parent / 'oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {row['name']: row for row in registry()}
        cls.sources = model.profiles()['sources']

    def case(self, name, variant):
        source = self.sources[name]
        record = self.oracle[name]
        declaration = dict(family=model.FAMILY, version=2, params=source['params'], oracle_text=record['oracle_text'], variants=[variant])
        return model.expand(name, source['registry']['csvid'], declaration, model.spec(declaration, record),
                            [variant], self.rows, self.oracle)[0]

    def last(self, steps, op, **fields):
        return next(step['value'] for step in reversed(steps) if step['op'] == op and all(step.get(k) == v for k, v in fields.items()))

    def test_exact_complete_sources_and_rejected_related_clauses(self):
        model.validate_witnesses(self.oracle, self.rows)
        self.assertEqual({pin['registry']['csvid'] for pin in self.sources.values()}, {13257, 18985, 14031, 1409, 12051, 9729})
        for name in ('Sygg, River Cutthroat', 'Crested Sunmare', 'Archfiend of Despair'):
            with self.assertRaises(ValueError):
                model.parameters(self.oracle[name])

    def test_life_gain_threshold_uses_real_spells_and_both_turns(self):
        for mode, tokens in [('gain_three', 0), ('split_gain', 1), ('opponent_gained', 0), ('gain_then_lose', 1)]:
            steps = self.case('Angelic Accord', mode + '_0')['steps']
            self.assertEqual(sum(step['op'] == 'bind_permanent_copy' and step['value'] == 18687 for step in steps), tokens)
        steps = self.case('Angelic Accord', 'opponent_history_1')['steps']
        self.assertIn(dict(op='turn', player=0, value=0), steps)
        self.assertIn(dict(op='choose_player', player=1, value=1), steps)
        self.assertIn(dict(op='assert_matching_permanents', player=1, index=18687, property='color', value=32), steps)
        self.assertIn(dict(op='assert_matching_permanents', player=1, index=18687, property='subtype_Angel', value=1), steps)

    def test_token_flying_waits_for_real_turns_and_rejects_the_ground_block(self):
        steps = self.case('Regal Bloodlord', 'token_flight_1')['steps']
        self.assertEqual(sum(step['op'] == 'advance_turn' for step in steps), 2)
        self.assertEqual(sum(step['op'] == 'untap_step' for step in steps), 2)
        self.assertEqual(sum(step['op'] == 'bind_permanent_copy' and step['value'] == 18693 for step in steps), 1)
        self.assertIn(dict(op='attack', ref='made_token_0', value=1), steps)
        self.assertIn(dict(op='block', ref='blocker', other='made_token_0', value=0), steps)
        self.assertEqual(self.last(steps, 'assert_life', player=0), 19)
        angel = self.case('Angelic Accord', 'token_flight_1')['steps']
        attacker = angel.index(dict(op='begin_attackers'))
        self.assertEqual(angel[attacker - 1], dict(op='turn', player=1, value=0))
        self.assertEqual(sum(step['op'] == 'untap_step' for step in angel[:attacker]), 2)

    def test_morbid_draw_is_optional_and_non_deaths_leave_library_unchanged(self):
        for name in ('Deathreap Ritual', 'Reaper from the Abyss'):
            supported = model.variants(model.parameters(self.oracle[name]))
            self.assertFalse(any(mode.startswith('next_turn_reset_') for mode in supported))
        for mode in ('own_history', 'opponent_history'):
            steps = self.case('Deathreap Ritual', mode + '_0')['steps']
            self.assertIn(dict(op='choose_trigger_source', player=0, ref='source'), steps)
            self.assertEqual(self.last(steps, 'assert_zone', player=0, zone='library'), 13)
            self.assertEqual(self.last(steps, 'assert_zone_card', player=0, zone='library', index=0), 188)
        for mode in ('decline_draw', 'exile_only', 'bounce_only', 'artifact_death'):
            steps = self.case('Deathreap Ritual', mode + '_0')['steps']
            self.assertFalse(any(step['op'] == 'choose_trigger_source' for step in steps))
            self.assertEqual(self.last(steps, 'assert_zone', player=0, zone='library'), 14)

    def test_asmira_uses_graveyard_owner_and_keeps_existing_counters_next_turn(self):
        name = 'Asmira, Holy Avenger'
        for mode, count in [('two_deaths', 2), ('enemy_death', 0), ('own_grave_other_controller', 1),
                            ('other_grave_own_controller', 0), ('next_turn_reset', 1)]:
            case = self.case(name, mode + '_1')
            steps = case['steps']
            self.assertEqual(self.last(steps, 'assert_card', ref='source', property='plus1_counters'), count)
            self.assertEqual(self.last(steps, 'assert_card', ref='source', property='power'), 2 + count)
            if 'controller' in mode:
                witness = next(card for card in case['setup'] if card['ref'] == 'history_victim')
                self.assertIn('opponent_owned', witness['state'])

    def test_reaper_only_has_legal_targets_and_cannot_destroy_indestructible(self):
        for mode, expected_movement in [('target_own', 0), ('indestructible', 1)]:
            steps = self.case('Reaper from the Abyss', mode + '_0')['steps']
            self.assertIn(dict(op='choose_target', player=0, ref='target'), steps)
            self.assertEqual(self.last(steps, 'assert_card', ref='target', property='in_play'), expected_movement)
            self.assertEqual(self.last(steps, 'assert_card', ref='demon', property='in_play'), 1)
        steps = self.case('Reaper from the Abyss', 'demon_only_1')['steps']
        self.assertFalse(any(step['op'] == 'choose_target' and step['ref'] in ('source', 'demon') for step in steps))

    def test_wound_reflection_doubles_aggregate_loss_not_net_change_and_replays(self):
        for mode, expected_life in [('pure_loss', 14), ('mixed_loss', 10), ('loss_then_gain', 18), ('own_loss', 20), ('two_reflections', 8)]:
            steps = self.case('Wound Reflection', mode + '_1')['steps']
            self.assertEqual(self.last(steps, 'assert_life', player=0), expected_life)
            if mode in ('pure_loss', 'mixed_loss'):
                self.assertEqual(sum(step['op'] == 'choose_player' for step in steps), int(mode == 'mixed_loss'))
        steps = self.case('Wound Reflection', 'end_snapshot_0')['steps']
        cut = steps.index(dict(op='capture'))
        self.assertEqual(steps.count(dict(op='cleanup')), 2)
        self.assertFalse(any(step['op'] == 'cast' for step in steps[cut:]))
        self.assertIn(dict(op='assert_recorded_state'), steps)


if __name__ == '__main__':
    unittest.main()
