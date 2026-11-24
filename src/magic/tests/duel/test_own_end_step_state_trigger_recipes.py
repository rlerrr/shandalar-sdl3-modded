"""Guard the whole clauses and live end-step state transitions."""

import json
import unittest

from generate_cases import registry
import own_end_step_state_trigger_recipes as model


class OwnEndStepStateTriggerTest(unittest.TestCase):
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

    def last_value(self, steps, op, **fields):
        return next(step['value'] for step in reversed(steps) if step['op'] == op and all(step.get(k) == v for k, v in fields.items()))

    def test_reviewed_single_clauses_and_exact_pinned_sources(self):
        model.validate_witnesses(self.oracle, self.rows)
        self.assertEqual({pin['registry']['csvid'] for pin in self.sources.values()}, {4912, 4913, 6927, 9209, 17047, 19794})
        for name in self.sources:
            record = self.oracle[name]
            with self.subTest(name=name):
                with self.assertRaises(ValueError):
                    model.parameters(dict(record, oracle_text=record['oracle_text'] + '\nDraw a card.'))
                with self.assertRaises(ValueError):
                    model.parameters(dict(record, oracle_text=record['oracle_text'].replace('your end step', 'each end step')))

    def test_well_gate_checks_own_untapped_lands_and_exact_ordered_draw(self):
        for mode, hand in [('no_lands', 3), ('enemy_untapped', 3), ('mixed_lands', 2), ('enemy_tapped', 2)]:
            steps = self.case('Well of Discovery', mode + '_0')['steps']
            self.assertEqual(self.last_value(steps, 'assert_zone', zone='hand', player=0), hand)
        steps = self.case('Well of Discovery', 'no_lands_0')['steps']
        cleanup = steps.index(dict(op='cleanup'))
        self.assertIn(dict(op='assert_zone_card', zone='library', index=0, player=0, value=91), steps[:cleanup])
        self.assertIn(dict(op='assert_zone_card', zone='library', index=0, player=0, value=188), steps[cleanup:])
        self.assertIn(dict(op='assert_zone_multiplicity', zone='hand', index=91, player=0, value=2), steps[cleanup:])

    def test_actual_tap_changes_gate_and_actual_opposing_step_does_not_trigger(self):
        steps = self.case('Well of Life', 'tap_last_land_1')['steps']
        tap = steps.index(dict(op='activate', ref='land', value=1))
        cleanup = steps.index(dict(op='cleanup'))
        self.assertIn(dict(op='assert_mana', player=1, color=3, value=1), steps[tap:cleanup])
        self.assertIn(dict(op='assert_life', player=1, value=22), steps[cleanup:])
        opposite = self.case('Well of Life', 'opponent_end_step_1')['steps']
        self.assertIn(dict(op='turn', player=0, value=0), opposite)
        self.assertEqual(self.last_value(opposite, 'assert_life', player=1), 20)
        untap = self.case('Well of Life', 'untap_last_land_0')['steps']
        self.assertIn(dict(op='cast', ref='twiddle', value=1), untap)
        self.assertEqual(self.last_value(untap, 'assert_life', player=0), 20)

    def test_nim_uses_current_tap_state_and_turn_sequence_dispatches_once(self):
        steps = self.case('Nim Abomination', 'attack_tap_0')['steps']
        self.assertIn(dict(op='attack', ref='source', value=1), steps)
        self.assertEqual(self.last_value(steps, 'assert_life', player=0), 20)
        self.assertEqual(self.last_value(steps, 'assert_life', player=1), 17)
        steps = self.case('Nim Abomination', 'turn_sequence_1')['steps']
        advances = [i for i, step in enumerate(steps) if step['op'] == 'advance_turn']
        self.assertEqual(len(advances), 2)
        self.assertIn(dict(op='untap_step', player=0), steps[advances[0]:advances[1]])
        self.assertIn(dict(op='untap_step', player=1), steps[advances[1]:])
        self.assertEqual(steps.count(dict(op='cleanup')), 1)
        self.assertEqual(self.last_value(steps, 'assert_life', player=1), 14)

    def test_untap_type_and_controller_scopes_include_kindred_permanent(self):
        merfolk = self.case('Merrow Commerce', 'kindred_source_tapped_1')['steps']
        self.assertEqual(self.last_value(merfolk, 'assert_card', ref='source', property='tapped'), 0)
        self.assertEqual(self.last_value(merfolk, 'assert_card', ref='merfolk', property='tapped'), 0)
        for ref in ('enemy_merfolk', 'nonmerfolk'):
            self.assertEqual(self.last_value(merfolk, 'assert_card', ref=ref, property='tapped'), 1)
        lands = self.case('Wilderness Reclamation', 'tapped_pair_0')['steps']
        for ref in ('land', 'land_two'):
            self.assertEqual(self.last_value(lands, 'assert_card', ref=ref, property='tapped'), 0)
        for ref in ('enemy_land', 'nonland'):
            self.assertEqual(self.last_value(lands, 'assert_card', ref=ref, property='tapped'), 1)

    def test_throne_counts_only_tapped_owned_creatures_and_replay_runs_real_phase(self):
        for mode, life in [('zero', 20), ('mixed_creatures', 18), ('three_creatures', 17), ('tap_mana_creature', 19), ('noncreatures_only', 20)]:
            steps = self.case('Throne of the God-Pharaoh', mode + '_1')['steps']
            self.assertEqual(self.last_value(steps, 'assert_life', player=0), life)
        replay = self.case('Well of Discovery', 'end_step_snapshot_1')['steps']
        self.assertEqual(replay.count(dict(op='cleanup')), 2)
        self.assertLess(replay.index(dict(op='capture')), replay.index(dict(op='cleanup')))
        self.assertIn(dict(op='restore'), replay)
        self.assertEqual(replay[-1], dict(op='assert_recorded_state'))


if __name__ == '__main__':
    unittest.main()
