"""Guard complete discounts, real payment, and paid cast-origin differences."""
import copy
import json
import unittest
from pathlib import Path

import artifact_discount_composition_recipes as model
from generate_cases import registry


class ArtifactDiscountCompositionTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        data = Path(__file__).parent / 'data'
        cls.oracle = json.loads((data / 'oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {row['name']: row for row in registry()}
        cls.declarations = {}
        for name, pin in model.pins()['sources'].items():
            path, = (data / 'cases/cards').glob(f"*/{pin['csvid']:05d}-*.json")
            cls.declarations[name] = json.loads(path.read_text(encoding='utf8'))['recipes'][0]

    def case(self, name, mode, owner):
        declaration = self.declarations[name]
        shape = model.spec(declaration, self.oracle[name])
        return model.expand(name, shape['csvid'], declaration, shape, [f'{mode}_{owner}'], self.rows, self.oracle)[0]

    @staticmethod
    def next_budget_before(steps, *, ref=None, op='cast'):
        index = next(i for i, row in enumerate(steps) if row['op'] == op and (ref is None or row.get('ref') == ref))
        funding = [row for row in steps[:index] if row['op'] == 'set_mana'][-6:]
        assert [row['color'] for row in funding] == list(range(6))
        return [row['value'] for row in funding]

    def count(self, steps, name, player, zone, value):
        self.assertIn(dict(op='assert_zone_multiplicity', player=player, zone=zone,
                           index=self.rows[name]['csvid'], value=value), steps)

    def test_complete_pins_and_generated_pairs(self):
        model.validate_witnesses(self.oracle, self.rows)
        for name, declaration in self.declarations.items():
            shape = model.spec(declaration, self.oracle[name])
            self.assertEqual(model.variants(shape), declaration['variants'])
            self.assertEqual(shape['clauses'], self.oracle[name]['oracle_text'].splitlines())
            self.assertEqual([x[:-2] for x in declaration['variants'][::2]], [x[:-2] for x in declaration['variants'][1::2]])
        self.assertEqual(sum(len(declaration['variants']) for declaration in self.declarations.values()), 130)

    def test_changed_or_partial_sources_are_rejected(self):
        name = 'Mycosynth Golem'
        record = self.oracle[name]
        self.assertEqual(model.fixed_cost(record), [11, 0, 0, 0, 0, 0])
        with self.assertRaises(ValueError):
            model.mechanism(dict(record, oracle_text=model.GRANT))
        with self.assertRaises(ValueError):
            model.mechanism(dict(record, type_line='Creature — Golem'))
        false_price = copy.deepcopy(self.declarations[name])
        false_price['params']['mechanism']['printed'][0] = 7
        with self.assertRaises(ValueError):
            model.spec(false_price, record)

    def test_affinity_uses_controlled_artifact_permanents_only(self):
        for owner in (0, 1):
            for name, full in [('Furnace Dragon', 6), ('Mycosynth Golem', 11)]:
                for mode, generic, success in [('full_price', full, 1), ('own_exact', full-2, 1), ('artifact_land', full-1, 1),
                                               ('short', full-3, 0), ('opponent_only', full-1, 0),
                                               ('nonartifact_only', full-1, 0), ('off_battlefield', full-1, 0)]:
                    steps = self.case(name, mode, owner)['steps']
                    self.assertEqual(self.next_budget_before(steps, ref='source')[0], generic)
                    self.assertIn(dict(op='cast', ref='source', value=success), steps)
                steps = self.case(name, 'zero_generic', owner)['steps']
                self.assertIn(dict(op='assert_mana', player=owner, color=0, value=2), steps[-90:])

    def test_actual_granted_affinity_uses_creature_and_control_scopes(self):
        for owner in (0, 1):
            for mode, generic in [('artifact_creature', 2), ('artifact_noncreature', 2), ('nonartifact', 1),
                                  ('opposing_caster', 4), ('intrinsic_affinity', 1), ('flat_composition', 1),
                                  ('multiple_reducers', 1), ('two_golems', 1), ('source_leaves', 4)]:
                steps = self.case('Mycosynth Golem', mode, owner)['steps']
                self.assertEqual(self.next_budget_before(steps, ref='probe')[0], generic, mode)
                self.assertIn(dict(op='cast', ref='probe', value=int(mode not in ('opposing_caster', 'source_leaves'))), steps)

    def test_flat_discounts_apply_to_all_artifact_spells_and_keep_color_floors(self):
        for owner in (0, 1):
            for name in ('Etherium Sculptor', 'Foundry Inspector'):
                for mode, generic in [('artifact_creature', 4), ('artifact_noncreature', 1), ('nonartifact', 1),
                                      ('opposing_caster', 4), ('multiple_reducers', 3), ('source_leaves', 4)]:
                    steps = self.case(name, mode, owner)['steps']
                    self.assertEqual(self.next_budget_before(steps, ref='probe')[0], generic)
                paid = self.case(name, 'colored_floor', owner)['steps']
                self.assertEqual(self.next_budget_before(paid, ref='probe'), [0, 1, 1, 0, 0, 0])
                wrong = self.case(name, 'wrong_colored_floor', owner)['steps']
                self.assertEqual(self.next_budget_before(wrong, ref='probe'), [4, 1, 0, 0, 0, 0])
                self.assertIn(dict(op='cast', ref='probe', value=0), wrong)

    def test_furnace_hand_cast_exiles_own_and_opposing_artifacts_only(self):
        for owner in (0, 1):
            steps = self.case('Furnace Dragon', 'mixed_exile', owner)['steps']
            self.assertEqual(self.next_budget_before(steps, ref='source'), [3, 0, 0, 0, 3, 0])
            settled = steps[steps.index(dict(op='resolve')):]
            for name, actor, zone, copies in [('Darksteel Relic', owner, 'exile', 2), ('Darksteel Citadel', owner, 'exile', 1),
                                              ('Myr Sire', 1-owner, 'exile', 1), ('Darksteel Relic', 1-owner, 'exile', 1),
                                              ('Sylvan Advocate', owner, 'battlefield', 1), ('Forest', 1-owner, 'battlefield', 1)]:
                self.count(settled, name, actor, zone, copies)
            wrong = self.case('Furnace Dragon', 'wrong_colored_floor', owner)['steps']
            self.assertEqual(self.next_budget_before(wrong, ref='source'), [3, 0, 0, 0, 2, 0])
            self.assertIn(dict(op='cast', ref='source', value=0), wrong)

    def test_furnace_noncast_entries_and_real_exile_permission_keep_artifacts(self):
        for owner in (0, 1):
            for mode, name in [('reanimate', 'Darksteel Relic'), ('blink', 'Renegade Map'), ('cast_from_exile', 'Darksteel Relic')]:
                result = self.case('Furnace Dragon', mode, owner)
                steps = result['steps']
                rebound = steps.index(dict(op='bind_permanent', player=owner, ref='source', value=6893))
                self.count(steps[rebound:], name, owner, 'battlefield', 2 if mode == 'cast_from_exile' else 1)
                self.count(steps[rebound:], 'Furnace Dragon', owner, 'battlefield', 1)
            exile = self.case('Furnace Dragon', 'cast_from_exile', owner)
            self.assertEqual(exile['setup'][0]['zone'], 'library')
            steps = exile['steps']
            self.assertEqual(self.next_budget_before(steps, ref='support'), [2, 0, 0, 0, 1, 0])
            self.assertEqual(self.next_budget_before(steps, op='play_exiled'), [4, 0, 0, 0, 3, 0])
            self.assertIn(dict(op='can_play_exiled', player=1-owner, index=6893, value=0), steps)
            self.assertIn(dict(op='play_exiled', player=owner, index=6893, value=1), steps)

    def test_full_budget_still_records_oracle_affinity_after_cast_origin(self):
        for owner in (0, 1):
            steps = self.case('Furnace Dragon', 'exile_origin_surplus', owner)['steps']
            self.assertEqual(self.next_budget_before(steps, op='play_exiled'), [6, 0, 0, 0, 3, 0])
            bind = steps.index(dict(op='bind_permanent', player=owner, ref='source', value=6893))
            residual = steps.index(dict(op='assert_mana', player=owner, color=0, value=2), bind)
            settled = steps[bind:residual]
            self.count(settled, 'Furnace Dragon', owner, 'battlefield', 1)
            self.count(settled, 'Darksteel Relic', owner, 'battlefield', 2)
            self.count(settled, 'Darksteel Relic', 1-owner, 'battlefield', 1)
            self.count(settled, 'Myr Sire', 1-owner, 'battlefield', 1)

    def test_flying_and_real_snapshot_replay(self):
        for owner in (0, 1):
            for mode, blocker, attacker, legal in [('flying_evasion', 'combatant', 'source', 0),
                                                    ('flying_reach', 'combatant', 'source', 1), ('blocks_flight', 'source', 'combatant', 1)]:
                steps = self.case('Furnace Dragon', mode, owner)['steps']
                self.assertIn(dict(op='block', ref=blocker, other=attacker, value=legal), steps)
                for earlier, later in [('advance_combat', 'combat_damage'), ('combat_damage', 'end_combat')]:
                    self.assertLess(steps.index(dict(op=earlier)), steps.index(dict(op=later)))
            for mode in ('paid_replay', 'exile_permission_replay'):
                steps = self.case('Furnace Dragon', mode, owner)['steps']
                capture, record, replay = [steps.index(dict(op=op)) for op in ('capture', 'record_state', 'ai_roundtrip')]
                self.assertEqual(steps[capture+1:record], steps[replay+1:-1])


if __name__ == '__main__':
    unittest.main()
