"""Guard whole Oracle and genuine attack/tap/block lifecycles."""

import json
import unittest

import attack_tap_creatures_recipes as model
from generate_cases import registry


class AttackTapCreatureTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle = json.loads((model.PROFILE_PATH.parent / 'oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {row['name']: row for row in registry()}

    def case(self, name, mode):
        record = self.oracle[name]
        declaration = dict(family=model.FAMILY, version=2, oracle_text=record['oracle_text'],
                           params=model.parameters(record), variants=[mode])
        return model.expand(name, self.rows[name]['csvid'], declaration, model.spec(declaration, record),
                            declaration['variants'], self.rows, self.oracle)[0]

    @staticmethod
    def values(steps, operation, **fields):
        return [s['value'] for s in steps if s['op'] == operation and all(s.get(key) == value for key, value in fields.items())]

    def test_exact_whole_oracle_and_independent_costs(self):
        names = {'Seasoned Marshal': (False, '{2}{W}{W}', 2), 'Alaborn Cavalier': (False, '{2}{W}{W}', 2),
                 'Flanking Troops': (False, '{2}{W}{W}', 2), 'Niblis of the Urn': (True, '{1}{W}', 1),
                 'Cho-Arrim Bruiser': (False, '{5}{W}', 3)}
        self.assertEqual(set(model.profile()['sources']), names.keys())
        model.validate_witnesses(self.oracle, self.rows)
        for name, expected in names.items():
            record = self.oracle[name]
            p = model.parameters(record)
            self.assertEqual((p['flying'], p['mana'], p['power']), expected)
            with self.assertRaises(ValueError):
                model.parameters(dict(record, oracle_text=record['oracle_text'].replace('target creature', 'target creature an opponent controls')))

    def test_foe_taps_between_real_attack_and_blocker_declaration(self):
        for owner in (0, 1):
            steps = self.case('Seasoned Marshal', f'foe_tap_{owner}')['steps']
            tap = [i for i, s in enumerate(steps) if s['op'] == 'assert_card' and s.get('ref') == 'foe' and s['property'] == 'tapped']
            ops = [s['op'] for s in steps]
            self.assertLess(ops.index('attack'), tap[0])
            self.assertLess(tap[0], ops.index('finish_attackers'))
            self.assertLess(ops.index('finish_attackers'), tap[1])
            self.assertLess(tap[1], ops.index('begin_blockers'))
            self.assertEqual([steps[i]['value'] for i in tap], [0, 1, 1])
            self.assertEqual(self.values(steps, 'block', ref='foe', other='source'), [0])
            self.assertEqual(self.values(steps, 'assert_life', player=1-owner), [18])

    def test_own_self_and_artifact_creatures_are_eligible_but_land_and_artifact_are_not(self):
        for owner in (0, 1):
            for variant, ref in [('own_target', 'own'), ('self_target', 'source'), ('already_tapped', 'foe')]:
                steps = self.case('Alaborn Cavalier', f'{variant}_{owner}')['steps']
                self.assertIn(dict(op='choose_target', player=owner, ref=ref), steps)
            steps = self.case('Alaborn Cavalier', f'target_type_{owner}')['steps']
            probe = [(s['other'], s['value']) for s in steps if s['op'] == 'can_target_creature']
            self.assertEqual(probe, [('land', 0), ('noncreature_artifact', 0), ('own', 1), ('foe', 1)])
            self.assertIn(dict(op='choose_target', player=owner, ref='own'), steps)
            self.assertEqual(self.values(steps, 'block', ref='foe', other='source'), [1])

    def test_independent_declines_and_denied_attacks(self):
        passed = self.case('Seasoned Marshal', 'decline_trigger_0')['steps']
        self.assertFalse(any(s['op'] in ('choose_trigger_source', 'choose_target', 'choose_optional_target') for s in passed))
        self.assertEqual(self.values(passed, 'block', ref='foe', other='source'), [1])
        for owner in (0, 1):
            canceled = self.case('Seasoned Marshal', f'decline_target_{owner}')['steps']
            self.assertIn(dict(op='choose_optional_target', player=owner, ref='foe'), canceled)
            self.assertEqual(self.values(canceled, 'assert_forced_target_choices')[0], 0)
            self.assertEqual(self.values(canceled, 'block', ref='foe', other='source'), [1])
            for variant in ('tapped_denied', 'fresh_cast_denied'):
                denied = self.case('Seasoned Marshal', f'{variant}_{owner}')['steps']
                self.assertEqual(self.values(denied, 'attack', ref='source'), [0])
                self.assertFalse(any(s['op'] == 'choose_trigger_source' for s in denied))
                self.assertEqual(self.values(denied, 'assert_card', ref='foe', property='tapped'), [0, 0])

    def test_multiple_sources_and_later_real_untap_and_attack(self):
        for owner in (0, 1):
            steps = self.case('Flanking Troops', f'two_sources_{owner}')['steps']
            self.assertEqual([s['ref'] for s in steps if s['op'] == 'choose_target'], ['foe', 'second_foe'])
            self.assertEqual([s['ref'] for s in steps if s['op'] == 'choose_trigger_source'], [] if owner else ['source', 'second_source'])
            self.assertEqual(self.values(steps, 'assert_life', player=1-owner), [16])
            again = self.case('Flanking Troops', f'repeat_turns_{owner}')['steps']
            self.assertEqual([s['player'] for s in again if s['op'] == 'untap_step'], [1-owner, owner])
            self.assertEqual(self.values(again, 'attack', ref='source'), [1, 1])
            self.assertEqual(self.values(again, 'assert_life', player=1-owner), [18, 16])

    def test_flying_contrasts_keep_other_target_separate(self):
        for owner in (0, 1):
            for variant, block in [('flying_ground', 0), ('flying_reach', 1)]:
                steps = self.case('Niblis of the Urn', f'{variant}_{owner}')['steps']
                self.assertIn(dict(op='choose_target', player=owner, ref='own'), steps)
                self.assertEqual(self.values(steps, 'block', ref='foe', other='source'), [block])
            aerial = self.case('Niblis of the Urn', f'block_flyer_{owner}')['steps']
            self.assertEqual(self.values(aerial, 'block', ref='source', other='flyer'), [1])
            self.assertFalse(any(s['op'] == 'choose_target' for s in aerial))

    def test_snapshot_replays_only_trigger_and_combat_tail(self):
        steps = self.case('Niblis of the Urn', 'snapshot_0')['steps']
        ops = [s['op'] for s in steps]
        self.assertLess(ops.index('attack'), ops.index('capture'))
        self.assertLess(ops.index('capture'), ops.index('finish_attackers'))
        self.assertEqual(ops.count('attack'), 1)
        self.assertEqual(ops.count('finish_attackers'), 2)
        self.assertEqual(ops[-1], 'assert_recorded_state')


    def test_bruiser_actual_zero_one_and_two_targets(self):
        for owner in (0, 1):
            for mode, count in [('zero_targets', 0), ('one_legal', 1), ('two_targets', 2), ('mixed_targets', 2)]:
                steps = self.case('Cho-Arrim Bruiser', f'{mode}_{owner}')['steps']
                declare = steps.index(dict(op='begin_attackers'))
                finish = steps.index(dict(op='finish_attackers'))
                inputs = steps[declare:finish]
                self.assertEqual(sum(s['op'] == 'choose_target' for s in inputs), count)
                self.assertEqual(sum(s['op'] == 'choose_trigger_source' for s in inputs), int(owner == 0))
                self.assertFalse(any(s['op'] == 'choose_cancel_target' for s in inputs))
                if mode == 'one_legal':
                    self.assertIn(dict(op='cast', ref='cloak', value=1), steps[:declare])
                    self.assertEqual(self.values(steps, 'can_target_creature', ref='source', other='source'), [0])
                    self.assertEqual(self.values(steps, 'can_target_creature', ref='source', other='foe'), [1])
                    self.assertEqual(self.values(steps, 'block', ref='foe', other='source'), [0])
            mixed = self.case('Cho-Arrim Bruiser', f'mixed_targets_{owner}')['steps']
            self.assertEqual(self.values(mixed, 'block', ref='second_foe', other='source'), [1])


if __name__ == '__main__':
    unittest.main()
