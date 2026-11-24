"""Focused guards for the distinct Bestow combat and attachment controls."""

from copy import deepcopy
import unittest

from bestow_cast_and_combat_basics_recipes import (
    FAMILY, classify, expand, profiles, spec, validate_witnesses, variants,
)


class BestowCastCombatTests(unittest.TestCase):
    def setUp(self):
        inventory = profiles()
        pins = inventory['sources'] | inventory['witnesses']
        self.rows = {name: row['registry'] for name, row in pins.items()}
        self.oracle = {name: row['oracle'] for name, row in pins.items()}
        self.rows.update({row['engine_aura_face']['name']: row['engine_aura_face']
                          for row in inventory['sources'].values()})

    def case(self, name, variant):
        record, csvid = self.oracle[name], self.rows[name]['csvid']
        typed = classify(record, csvid)
        declaration = dict(family=FAMILY, version=2, params=typed, oracle_text=record['oracle_text'],
                           variants=[variant])
        model = spec(declaration, record)
        case, = expand(name, csvid, declaration, model, [variant], self.rows, self.oracle)
        return case

    def test_pending_replay_restores_genuine_bestow_target_loss(self):
        case = self.case('Baleful Eidolon', 'pending_replay_1')
        steps = case['steps']
        op = [s['op'] for s in steps]
        self.assertEqual(op.count('cast_response'), 2)
        self.assertEqual(op.count('combat_damage'), 2)
        self.assertLess(op.index('cast'), op.index('capture'))
        self.assertLess(op.index('resolve_response'), op.index('restore'))
        self.assertEqual(op[-1], 'assert_recorded_state')

    def test_vigilance_controls_a_real_later_block(self):
        steps = self.case('Observant Alseid', 'vigilance_defends_0')['steps']
        block = steps.index(dict(op='block', ref='host', other='retaliator', value=1))
        self.assertTrue(any(s == dict(op='untap_step', player=1) for s in steps[:block]))
        self.assertFalse(any(s == dict(op='untap_step', player=0) for s in steps[:block]))
        self.assertIn(dict(op='assert_card', ref='host', property='tapped', value=0), steps[:block])

    def test_aura_loss_removes_trample_even_with_valid_attacker_and_blocker(self):
        case = self.case("Nylea's Emissary", 'destroy_aura_1')
        steps = case['steps']
        declaration = steps.index(dict(op='begin_attackers'))
        host = [s['value'] for s in steps[:declaration]
                if s.get('ref') == 'host' and s.get('property') == 'power']
        self.assertEqual(host[-2:], [4, 1])
        self.assertIn(dict(op='assert_life', player=0, value=20), steps[declaration:])
        self.assertIn(dict(op='assert_zone_multiplicity', player=1, zone='graveyard', index=13498, value=1),
                      steps[declaration:])

    def test_exact_whole_oracle_and_physical_bestow_pair_are_required(self):
        record = self.oracle['Nimbus Naiad']
        model = classify(record, 13492)
        declaration = dict(family=FAMILY, version=2, params=model, oracle_text=record['oracle_text'],
                           variants=variants(model))
        self.assertEqual(spec(declaration, record)['bestow_cost'], '{4}{U}')
        wrong = deepcopy(declaration)
        wrong['params']['bestow_cost'] = '{3}{U}'
        with self.assertRaises(ValueError):
            spec(wrong, record)
        bad_rows = dict(self.rows, **{'Nimbus Naiad Bestowed':
                                     dict(self.rows['Nimbus Naiad Bestowed'], iid=99999)})
        with self.assertRaises(ValueError):
            validate_witnesses(self.oracle, bad_rows)
