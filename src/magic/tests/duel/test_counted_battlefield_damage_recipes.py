"""Check the printed payment and independent resolution-count ledger."""
import copy
import json
import unittest
from pathlib import Path

import counted_battlefield_damage_recipes as model
from generate_cases import registry


class CountedBattlefieldDamageTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        data = Path(__file__).parent / 'data'
        cls.oracle = json.loads((data / 'oracle.json').read_bytes())['cards']
        cls.rows = {row['name']: row for row in registry()}
        cls.declarations = {}
        for name, pin in model.pins()['sources'].items():
            path, = (data / 'cases/cards').glob(f"*/{pin['csvid']:05d}-*.json")
            cls.declarations[name] = json.loads(path.read_bytes())['recipes'][0]

    def case(self, name, mode, owner):
        declaration = self.declarations[name]
        shape = model.spec(declaration, self.oracle[name])
        return model.expand(name, shape['csvid'], declaration, shape, [f'{mode}_{owner}'], self.rows, self.oracle)[0]

    @staticmethod
    def observed(steps, ref='target', prop='damage'):
        return [step['value'] for step in steps if step['op'] == 'assert_card' and step['ref'] == ref and step['property'] == prop]

    def assert_zone(self, steps, player, zone, name, count=1):
        self.assertIn(dict(op='assert_zone_multiplicity', player=player, zone=zone,
                           index=self.rows[name]['csvid'], value=count), steps)

    def test_whole_pins_cost_timing_and_mirror(self):
        model.validate_witnesses(self.oracle, self.rows)
        self.assertEqual(sum(len(decl['variants']) for decl in self.declarations.values()), 228)
        for name, declaration in self.declarations.items():
            shape = model.spec(declaration, self.oracle[name])
            self.assertEqual(declaration['variants'], model.variants(shape))
            self.assertEqual(shape['clauses'], [self.oracle[name]['oracle_text']])
            self.assertEqual(shape['instant'], self.oracle[name]['type_line'] == 'Instant')
            self.assertEqual('offturn_response_0' in declaration['variants'], shape['instant'])
            self.assertEqual([v[:-2] for v in declaration['variants'][::2]], [v[:-2] for v in declaration['variants'][1::2]])

    def test_reject_other_target_count_clause_and_manipulated_cost(self):
        name = 'Ground Assault'
        oracle = self.oracle[name]
        for wrong in ('target player', 'target creature or player', 'target creature an opponent controls'):
            invalid = dict(oracle, oracle_text=oracle['oracle_text'].replace('target creature', wrong))
            with self.assertRaises(ValueError):
                model.mechanism(invalid)
        altered = copy.deepcopy(self.declarations[name])
        altered['params']['mechanism']['printed'][3] = 0
        with self.assertRaises(ValueError):
            model.spec(altered, oracle)

    def test_zero_nonnative_count_and_real_matching_types(self):
        for name, mixed in [('Seismic Strike', 2), ('Ground Assault', 3), ('Outnumber', 2)]:
            for owner in (0, 1):
                for mode, expected in [('zero', 0), ('opponent_only', 0), ('other_zones', 0), ('own_mixed', mixed), ('own_target', 1)]:
                    self.assertEqual(self.observed(self.case(name, mode, owner)['steps'])[-1], expected)
        self.assertTrue(model.counted(self.oracle['Taiga'], 'mountain'))
        self.assertFalse(model.counted(self.oracle['Forest'], 'mountain'))
        self.assertTrue(model.counted(self.oracle['Darksteel Citadel'], 'land'))
        self.assertFalse(model.counted(self.oracle['Darksteel Relic'], 'land'))
        self.assertTrue(model.counted(self.oracle['Soldier'], 'creature'))

    def test_live_own_and_enemy_updates_before_source_resolution(self):
        for name, own_gain in [('Seismic Strike', 1), ('Ground Assault', 1), ('Outnumber', 2)]:
            for owner in (0, 1):
                for mode, expected, player in [('gain_own', own_gain, owner), ('gain_opponent', 1, 1-owner),
                                                ('lose_own', 0, owner), ('lose_opponent', 1, 1-owner)]:
                    case = self.case(name, mode, owner)
                    support = next(row for row in case['setup'] if row['ref'] == 'support')
                    self.assertEqual(support['player'], player)
                    steps = case['steps']
                    order = [dict(op='cast', ref='source', value=1), dict(op='announce_spell', ref='source'),
                             dict(op='cast_response', ref='support', value=1), dict(op='resolve_response')]
                    self.assertEqual(sorted(order, key=steps.index), order)
                    settled = steps[steps.index(order[-1]):]
                    self.assertEqual(self.observed(settled)[-1], expected)
                    self.assert_zone(settled, player, 'graveyard', support['name'])
                    if support['name'] == 'Growth Spiral':
                        self.assert_zone(settled, player, 'hand', 'Island')
                    if support['name'] == 'Raise the Alarm':
                        self.assert_zone(settled, player, 'battlefield', 'Soldier', 2)

    def test_real_nonmountain_gain_does_not_change_mountain_damage(self):
        for owner in (0, 1):
            steps = self.case('Spitting Earth', 'gain_nonmountain', owner)['steps']
            select = steps.index(dict(op='choose_zone_card', player=owner, value=91))
            resolve = steps.index(dict(op='resolve_response'))
            self.assertLess(select, resolve)
            self.assert_zone(steps[resolve:], owner, 'battlefield', 'Forest')
            self.assertEqual(self.observed(steps[resolve:])[-1], 1)

    def test_target_boundaries_exact_lethal_and_cleanup(self):
        for owner in (0, 1):
            for name in ('Ground Assault', 'Outnumber'):
                self.assert_zone(self.case(name, 'exact_lethal', owner)['steps'], 1-owner, 'graveyard', 'Elvish Mystic')
                self.assert_zone(self.case(name, 'target_bounced', owner)['steps'], 1-owner, 'hand', 'Dazzling Ramparts')
                for mode in ('target_hexproof', 'countered'):
                    steps = self.case(name, mode, owner)['steps']
                    self.assertEqual(self.observed(steps)[-1], 0)
                cleanup = self.case(name, 'cleanup_mark', owner)['steps']
                self.assertEqual(self.observed(cleanup), [0, 1, 0])
                for mode, query in [('no_legal_target', 0), ('wrong_color', 1), ('unfunded', 1)]:
                    denied = self.case(name, mode, owner)['steps']
                    self.assertIn(dict(op='can_cast', ref='source', value=query), denied)
                    self.assertIn(dict(op='cast', ref='source', value=0), denied)
                    self.assertFalse(any(step['op'] == 'choose_target' for step in denied))

    def test_only_instants_cast_offturn_as_actual_responses(self):
        for name in ('Seismic Strike', 'Stonefury', 'Outnumber'):
            for owner in (0, 1):
                steps = self.case(name, 'offturn_response', owner)['steps']
                ordered = [dict(op='advance_turn'), dict(op='cast', ref='support', value=1),
                           dict(op='announce_spell', ref='support'), dict(op='cast_response', ref='source', value=1), dict(op='resolve_response')]
                self.assertEqual(sorted(ordered, key=steps.index), ordered)
                self.assertEqual(self.observed(steps)[-1], 1)
                self.assert_zone(steps[steps.index(ordered[-1]):], 1-owner, 'hand', 'Island')

    def test_pending_replay_copies_update_and_postresolution_damage(self):
        for name in self.declarations:
            for owner in (0, 1):
                steps = self.case(name, 'replay_update', owner)['steps']
                capture, record, ai = [steps.index(dict(op=op)) for op in ('capture','record_state','ai_roundtrip')]
                self.assertEqual(steps[capture+1:record], steps[ai+1:-1])
                self.assertIn(dict(op='resolve_response'), steps[capture:record])
                self.assertEqual(self.observed(steps[ai+1:])[-1], 3 if name == 'Outnumber' else 2)


if __name__ == '__main__':
    unittest.main()
