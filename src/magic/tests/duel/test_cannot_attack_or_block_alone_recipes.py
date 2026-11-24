"""Guards for real companion declarations and independent Flying eligibility."""

import json
import unittest

from generate_cases import registry
import cannot_attack_or_block_alone_recipes as model


class CannotAttackOrBlockAloneTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle = json.loads((model.PROFILE_PATH.parent / 'oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {row['name']: row for row in registry()}

    def case(self, name, mode):
        record = self.oracle[name]
        declaration = dict(family=model.FAMILY, version=2, oracle_text=record['oracle_text'],
                           params=model.parameters(record), variants=[mode])
        return model.expand(name, self.rows[name]['csvid'], declaration, model.spec(declaration, record),
                            [mode], self.rows, self.oracle)[0]

    @staticmethod
    def values(steps, op, **fields):
        return [step['value'] for step in steps if step['op'] == op and all(step.get(key) == value for key, value in fields.items())]

    def test_whole_clause_and_role_parameters(self):
        model.validate_witnesses(self.oracle, self.rows)
        expected = {5556: (3, 4, '{2}{R}', False), 10436: (2, 2, '{R}', False),
                    13747: (2, 1, '{W}', True), 3397: (3, 3, '{1}{R}', False)}
        for name, pin in model.profiles()['sources'].items():
            record = self.oracle[name]
            p = model.parameters(record)
            if 'bestow_cost' in p:
                continue
            self.assertEqual((p['power'], p['toughness'], p['printed_mana'], p['flying']), expected[pin['registry']['csvid']])
            for text in (record['oracle_text'].replace(' or block', ''), record['oracle_text'] + '\nHaste'):
                with self.assertRaises(ValueError):
                    model.parameters(dict(record, oracle_text=text))

    def test_attack_requires_actual_companion_declaration(self):
        for owner in (0, 1):
            solo = self.case('Ember Beast', f'attack_solo_{owner}')['steps']
            self.assertEqual(self.values(solo, 'attack', ref='source'), [0])
            self.assertEqual(self.values(solo, 'assert_life', player=1 - owner)[-1], 20)
            idle = self.case('Ember Beast', f'attack_idle_ally_{owner}')['steps']
            self.assertEqual(self.values(idle, 'attack', ref='source'), [0])
            self.assertFalse(self.values(idle, 'attack', ref='ally'))
            for mode in ('attack_tapped_ally', 'attack_sick_ally', 'attack_defender_ally'):
                steps = self.case('Ember Beast', f'{mode}_{owner}')['steps']
                self.assertEqual(self.values(steps, 'attack', ref='ally'), [0])
                self.assertEqual(self.values(steps, 'attack', ref='source'), [0])
            group = self.case('Ember Beast', f'attack_with_ally_{owner}')['steps']
            self.assertLess(group.index(dict(op='attack', ref='ally', value=1)), group.index(dict(op='attack', ref='source', value=1)))
            self.assertEqual(self.values(group, 'assert_life', player=1 - owner)[-1], 16)
            self.assertIn(dict(op='assert_card', ref='source', property='tapped', value=1), group)

    def test_real_blocks_allow_companion_on_either_attacker(self):
        for owner in (0, 1):
            for mode, other in [('block_with_ally_same', 'enemy'), ('block_with_ally_different', 'enemy_ally')]:
                steps = self.case('Jackal Familiar', f'{mode}_{owner}')['steps']
                self.assertLess(steps.index(dict(op='block', ref='ally', other=other, value=1)),
                                steps.index(dict(op='block', ref='source', other='enemy', value=1)))
                self.assertIn(dict(op='assert_card', ref='source', property='blocking_count', value=1), steps)
                self.assertEqual(self.values(steps, 'assert_life', player=owner)[-1], 20)
            for mode in ('block_solo', 'block_idle_ally', 'block_tapped_ally', 'block_cannot_block_ally', 'block_wrong_air_ally'):
                steps = self.case('Jackal Familiar', f'{mode}_{owner}')['steps']
                self.assertEqual(self.values(steps, 'block', ref='source'), [0])
                self.assertIn(dict(op='assert_card', ref='source', property='blocking_count', value=0), steps)

    def test_post_declaration_removal_does_not_undo_legal_attack_or_block(self):
        attack = self.case('Mogg Flunkies', 'attack_remove_ally_after_1')['steps']
        self.assertLess(attack.index(dict(op='finish_attackers')), attack.index(dict(op='cast', ref='bounce', value=1)))
        self.assertEqual(self.values(attack, 'assert_life', player=0)[-1], 17)
        block = self.case('Mogg Flunkies', 'block_remove_ally_after_1')['steps']
        self.assertLess(block.index(dict(op='advance_combat')), block.index(dict(op='cast', ref='bounce', value=1)))
        self.assertEqual(self.values(block, 'assert_life', player=1)[-1], 20)
        for mode, boundary in [('attack_remove_ally_before', 'begin_attackers'), ('block_remove_ally_before', 'begin_blockers')]:
            steps = self.case('Mogg Flunkies', mode + '_0')['steps']
            self.assertLess(steps.index(dict(op='cast', ref='bounce', value=1)), steps.index(dict(op=boundary)))
            self.assertIn(dict(op='bind_hand', player=0, ref='ally', value=1565), steps)

    def test_flying_and_reach_are_independent_of_companion(self):
        ground = self.case('Loyal Pegasus', 'attack_ground_block_0')['steps']
        self.assertIn(dict(op='block', ref='enemy', other='source', value=0), ground)
        self.assertEqual(self.values(ground, 'assert_life', player=1)[-1], 17)
        reach = self.case('Loyal Pegasus', 'attack_reach_block_0')['steps']
        self.assertIn(dict(op='block', ref='enemy', other='source', value=1), reach)
        self.assertEqual(self.values(reach, 'assert_life', player=1)[-1], 19)
        flying = self.case('Loyal Pegasus', 'block_air_with_ally_1')['steps']
        ground = self.case('Ember Beast', 'block_air_with_ally_1')['steps']
        self.assertEqual(self.values(flying, 'block', ref='source'), [1])
        self.assertEqual(self.values(ground, 'block', ref='source'), [0])
        self.assertEqual(self.values(ground, 'assert_life', player=1)[-1], 18)

    def test_sickness_and_exact_paid_cost_are_observed(self):
        for mode, legal, untaps in [('attack_paid_fresh', 0, []), ('attack_paid_next_turn', 1, [0, 1]),
                                    ('block_paid_fresh', 1, [0])]:
            steps = self.case('Loyal Pegasus', mode + '_1')['steps']
            self.assertIn(dict(op='set_mana', player=1, color=5, value=1), steps)
            self.assertIn(dict(op='cast', ref='source', value=1), steps)
            op = 'block' if mode.startswith('block') else 'attack'
            self.assertEqual(self.values(steps, op, ref='source'), [legal])
            self.assertEqual([step['player'] for step in steps if step['op'] == 'untap_step'], untaps)
        for mode in ('unfunded', 'wrong_color'):
            steps = self.case('Ember Beast', mode + '_0')['steps']
            self.assertEqual(self.values(steps, 'cast', ref='source'), [0])
            self.assertFalse(any(step['op'] == 'begin_attackers' for step in steps))

    def test_replay_retains_already_selected_companion_once(self):
        for mode, op in [('attack_replay', 'attack'), ('block_replay', 'block')]:
            steps = self.case('Jackal Familiar', mode + '_1')['steps']
            boundary = steps.index(dict(op='capture'))
            self.assertEqual(self.values(steps[:boundary], op, ref='ally'), [1])
            self.assertEqual(self.values(steps[boundary:], op, ref='ally'), [])
            self.assertEqual(self.values(steps[boundary:], op, ref='source'), [1, 1])
            self.assertIn(dict(op='assert_recorded_state'), steps)


class SightlessBrawlerPreviewTest(unittest.TestCase):
    """Bestow composition guards, loaded only in the private preview."""

    case = CannotAttackOrBlockAloneTest.case
    values = staticmethod(CannotAttackOrBlockAloneTest.values)

    @classmethod
    def setUpClass(cls):
        cls.oracle = json.loads((model.PROFILE_PATH.parent / 'oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {row['name']: row for row in registry()}

    def test_brawler_preserves_distinct_ordinary_and_bestow_clauses(self):
        record = self.oracle['Sightless Brawler']
        self.assertEqual(model.parameters(record), dict(normal_cost='{1}{W}', bestow_cost='{4}{W}', bonus=[3, 2], creature=[3, 2]))
        for token in ("This creature can't attack alone.", " and can't attack alone"):
            with self.assertRaises(ValueError):
                model.parameters(dict(record, oracle_text=record['oracle_text'].replace(token, '', 1)))

    def test_brawler_intrinsic_and_host_groups(self):
        for who in (0, 1):
            for mode, ref, allowed, life, defender in (
                ('regular_solo', 'source', 0, 20, 1 - who), ('regular_group', 'source', 1, 16, 1 - who),
                ('bestow_own_solo', 'host', 0, 20, 1 - who), ('bestow_own_group', 'host', 1, 15, 1 - who),
                ('bestow_foreign_solo', 'host', 0, 20, who), ('bestow_foreign_group', 'host', 1, 15, who),
            ):
                steps = self.case('Sightless Brawler', f'{mode}_{who}')['steps']
                self.assertEqual(self.values(steps, 'attack', ref=ref), [allowed])
                self.assertEqual(self.values(steps, 'attack', ref='ally'), [1] if allowed else [])
                self.assertEqual(self.values(steps, 'assert_life', player=defender)[-1], life)
                if ref == 'host':
                    self.assertIn(dict(op='choose_dialog', player=who, value=0), steps)
                    self.assertIn(dict(op='choose_target', player=who, ref='host'), steps)
                    self.assertEqual(self.values(steps, 'assert_card', ref='host', property='power')[-1], 4)
                    self.assertEqual(self.values(steps, 'assert_card', ref='host', property='toughness')[-1], 3)

    def test_brawler_blocking_is_independent_and_detachment_restores_creature(self):
        for mode, ref in [('regular_fresh_block', 'source'), ('bestow_own_block', 'host')]:
            steps = self.case('Sightless Brawler', mode + '_1')['steps']
            self.assertEqual(self.values(steps, 'block', ref=ref, other='attacker'), [1])
            self.assertEqual(self.values(steps, 'assert_card', ref='attacker', property='in_play'), [0])
        for mode, response in [('detached_solo', False), ('pending_lost_solo', True)]:
            steps = self.case('Sightless Brawler', mode + '_1')['steps']
            self.assertIn(dict(op='cast_response' if response else 'cast', ref='helper', value=1), steps)
            self.assertEqual(self.values(steps, 'assert_card', ref='source', property='csvid')[-1], 13972)
            self.assertEqual(self.values(steps, 'attack', ref='source'), [0])
        steps = self.case('Sightless Brawler', 'aura_destroyed_releases_host_1')['steps']
        self.assertIn(dict(op='choose_target', player=0, ref='source'), steps)
        self.assertEqual(self.values(steps, 'assert_card', ref='host', property='power')[-1], 1)
        self.assertEqual(self.values(steps, 'attack', ref='host'), [1])

    def test_real_choices_cast_failure_and_pending_replay(self):
        for mode, choice in [('cancel_bestow', 2), ('cancel_bestow_target', 0)]:
            steps = self.case('Sightless Brawler', mode + '_1')['steps']
            self.assertIn(dict(op='choose_dialog', player=1, value=choice), steps)
            self.assertEqual(self.values(steps, 'cast', ref='source'), [0])
        steps = self.case('Sightless Brawler', 'pending_bestow_replay_1')['steps']
        self.assertEqual(self.values(steps, 'cast', ref='source'), [1])
        self.assertEqual(self.values(steps, 'attack', ref='host'), [1, 1])
        self.assertIn(dict(op='assert_recorded_state'), steps)


if __name__ == '__main__':
    unittest.main()
