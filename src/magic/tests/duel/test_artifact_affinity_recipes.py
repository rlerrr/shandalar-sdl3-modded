import json
import unittest
from pathlib import Path

from artifact_affinity_recipes import FAMILY, RULE, classify, expand, profiles, spec, validate_witnesses, variants
from generate_cases import registry
from recipes import COLORS


class ArtifactAffinityRecipesTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle = json.loads((Path(__file__).parent / 'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {row['name']: row for row in registry()}

    def declaration(self, name, selected):
        record = self.oracle[name]
        return dict(family=FAMILY, version=2, oracle_text=record['oracle_text'], params=classify(record), variants=selected)

    def case(self, name, selected):
        declaration = self.declaration(name, [selected])
        case, = expand(name, self.rows[name]['csvid'], declaration, spec(declaration, self.oracle[name]),
                       [selected], self.rows, self.oracle)
        return case

    def actions(self, steps, op, **fields):
        return [step for step in steps if step['op'] == op and all(step.get(key) == value for key, value in fields.items())]

    def values(self, steps, op, **fields):
        return [step['value'] for step in self.actions(steps, op, **fields)]

    def before(self, steps, left_op, right_op, left_ref, right_ref):
        left, = self.actions(steps, left_op, ref=left_ref)
        right, = self.actions(steps, right_op, ref=right_ref)
        self.assertLess(steps.index(left), steps.index(right))

    def test_complete_cost_grammar_and_exact_typed_source_profiles(self):
        validate_witnesses(self.oracle, self.rows)
        costs = {1624: (4, 0), 1625: (7, 0), 1626: (6, 2), 1739: (5, 1)}
        self.assertEqual({self.rows[name]['csvid'] for name in profiles()['sources']}, set(costs))
        for name in profiles()['sources']:
            record = self.oracle[name]
            params = classify(record)
            self.assertEqual(tuple(params['printed'][i] for i in (0, COLORS['U'])), costs[self.rows[name]['csvid']])
            self.assertEqual(len(set(variants(params))), 40 if params['flying'] else 32)
            declaration = self.declaration(name, ['own_exact_0'])
            spec(declaration, record)
            with self.assertRaises(ValueError):
                spec(dict(declaration, params=dict(params, power=float(params['power']))), record)
            with self.assertRaises(ValueError):
                classify(dict(record, oracle_text=RULE + '\nDraw a card.'))

    def test_price_depends_on_controlled_artifact_type_and_battlefield(self):
        for owner in (0, 1):
            for mode, generic, allowed in (('full_price', 4, 1), ('own_exact', 2, 1), ('short_own', 2, 0),
                                           ('mixed_controllers', 3, 1), ('opponent_only', 3, 0),
                                           ('other_permanents', 3, 0), ('other_zones', 3, 0),
                                           ('artifact_land', 3, 1), ('tapped_artifact', 3, 1)):
                case = self.case('Frogmite', f'{mode}_{owner}')
                steps = case['steps']
                self.assertEqual(self.values(steps, 'cast', ref='source'), [allowed])
                self.assertEqual(self.values(steps, 'set_mana', player=owner, color=0)[-1], generic)
                self.assertEqual(self.values(steps, 'assert_mana', player=owner, color=0)[-1], 0 if allowed else generic)
                if mode == 'other_zones':
                    self.assertEqual({entry['zone'] for entry in case['setup']}, {'hand', 'graveyard', 'exile', 'library'})
                if mode == 'tapped_artifact':
                    self.assertIn(dict(name='Ornithopter', zone='battlefield', state=['tapped'], player=owner, ref='changing'), case['setup'])

    def test_generic_floor_leaves_surplus_and_cannot_remove_blue_pips(self):
        for name, blue in (('Qumulox', 2), ('Somber Hoverguard', 1)):
            for owner in (0, 1):
                paid = self.case(name, f'floor_surplus_{owner}')['steps']
                self.assertEqual(self.values(paid, 'cast', ref='source'), [1])
                self.assertEqual(self.values(paid, 'assert_mana', player=owner, color=0)[-1], 2)
                self.assertEqual(self.values(paid, 'set_mana', player=owner, color=COLORS['U'])[-1], blue)
                self.assertEqual(self.values(paid, 'assert_mana', player=owner, color=COLORS['U'])[-1], 0)
                denied = self.case(name, f'colored_floor_short_{owner}')['steps']
                self.assertEqual(self.values(denied, 'cast', ref='source'), [0])
                self.assertEqual(self.values(denied, 'assert_mana', player=owner, color=COLORS['U'])[-1], blue - 1)

    def test_actual_generic_payment_from_wrong_symbol_preserves_blue_pip(self):
        for owner in (0, 1):
            steps = self.case('Somber Hoverguard', f'generic_from_black_{owner}')['steps']
            choices = self.actions(steps, 'choose_mana', player=owner, color=COLORS['B'])
            self.assertEqual(len(choices), 4 if owner == 0 else 0)
            self.assertEqual(self.values(steps, 'set_mana', player=owner, color=COLORS['U'])[-1], 1)
            self.assertEqual(self.values(steps, 'set_mana', player=owner, color=COLORS['B'])[-1], 4)
            self.assertEqual(self.values(steps, 'assert_mana', player=owner, color=COLORS['B'])[-1], 0)
            if choices:
                self.assertLess(steps.index(choices[-1]), steps.index(self.actions(steps, 'cast', ref='source')[0]))

    def test_real_entry_removal_and_control_transfer_update_the_next_price(self):
        entered = self.case('Frogmite', 'enter_artifact_0')['steps']
        self.before(entered, 'cast', 'cast', 'changing', 'source')
        self.assertEqual(self.values(entered, 'set_mana', player=0, color=0)[-1], 3)
        removed = self.case('Myr Enforcer', 'remove_artifact_1')['steps']
        self.before(removed, 'cast', 'cast', 'shatter', 'source')
        self.assertEqual(self.values(removed, 'cast', ref='source'), [0])
        self.assertEqual(self.values(removed, 'assert_zone_multiplicity', zone='graveyard', player=1, index=514)[-1], 1)
        self.assertEqual(self.values(removed, 'assert_mana', player=1, color=0)[-1], 6)
        stolen = self.case('Myr Enforcer', 'stolen_artifact_1')['steps']
        self.before(stolen, 'cast', 'bind_permanent_copy', 'steal', 'stolen')
        self.before(stolen, 'bind_permanent_copy', 'cast', 'stolen', 'source')
        self.assertEqual(self.values(stolen, 'assert_zone_multiplicity', zone='battlefield', player=0, index=11586)[-1], 0)
        self.assertEqual(self.values(stolen, 'assert_zone_multiplicity', zone='battlefield', player=1, index=11586)[-1], 1)

    def test_first_resolution_only_discounts_later_cast_if_it_is_an_artifact(self):
        for name, total, second_price in (('Frogmite', 5, 2), ('Qumulox', 10, 5)):
            steps = self.case(name, 'second_cast_0')['steps']
            self.before(steps, 'cast', 'cast', 'source', 'second')
            self.assertEqual(self.values(steps, 'set_mana', player=0, color=0)[-1], total)
            self.assertIn(second_price, self.values(steps, 'assert_mana', player=0, color=0))
            self.assertEqual(self.values(steps, 'assert_mana', player=0, color=0)[-1], 0)

    def test_flying_in_live_attack_and_block_declarations(self):
        for owner in (0, 1):
            for mode, att, block, allowed, life in (('ground_block_denied', 'source', 'foe', 0, 15),
                                                    ('reach_block_allowed', 'source', 'foe', 1, 20),
                                                    ('block_flyer', 'foe', 'source', 1, 20)):
                steps = self.case('Qumulox', f'{mode}_{owner}')['steps']
                self.before(steps, 'cast', 'attack', 'source', att)
                self.assertEqual(self.values(steps, 'block', ref=block, other=att), [allowed])
                self.assertEqual(self.values(steps, 'assert_life', player=1-owner)[-1], life)

    def test_pending_replay_reuses_the_exact_paid_cast(self):
        steps = self.case('Qumulox', 'pending_snapshot_1')['steps']
        ops = [step['op'] for step in steps]
        capture, record, restore = [ops.index(op) for op in ('capture', 'record_state', 'restore')]
        self.assertLess(ops.index('cast'), capture)
        self.assertEqual(ops[capture + 1], 'resolve')
        self.assertEqual(steps[capture + 1:record], steps[restore + 1:-1])
        self.assertFalse(self.actions(steps[capture:], 'set_mana'))

    def test_thoughtcast_uses_affinity_and_draws_the_exact_top_two(self):
        for owner in (0, 1):
            steps = self.case('Thoughtcast', f'reduced_draw_{owner}')['steps']
            self.assertEqual(self.values(steps, 'set_mana', player=owner, color=0)[-1], 2)
            self.assertEqual(self.values(steps, 'set_mana', player=owner, color=COLORS['U'])[-1], 1)
            self.assertEqual(self.values(steps, 'cast', ref='source'), [1])
            for name in ('Plains', 'Island'):
                csvid = self.rows[name]['csvid']
                self.assertEqual(self.values(steps, 'assert_zone_multiplicity', player=owner, zone='hand', index=csvid)[-1], 1)
            self.assertEqual(self.values(steps, 'assert_zone_card', player=owner, zone='library', index=0)[-1], self.rows['Mountain']['csvid'])
            self.assertEqual(self.values(steps, 'assert_zone', player=1-owner, zone='library')[-1], 1)
            last = self.case('Thoughtcast', f'last_two_{owner}')['steps']
            self.assertEqual(self.values(last, 'assert_zone', player=owner, zone='library')[-1], 0)
            self.assertEqual(self.values(last, 'assert_life', player=owner)[-1], 20)

    def test_thoughtcast_two_casts_spend_and_draw_separately(self):
        steps = self.case('Thoughtcast', 'two_draws_0')['steps']
        self.before(steps, 'cast', 'cast', 'source', 'second')
        self.assertEqual(self.values(steps, 'assert_zone', player=0, zone='library'), [4, 2, 0])
        self.assertEqual(self.values(steps, 'assert_zone_multiplicity', player=0, zone='graveyard', index=1741)[-1], 2)

    def test_tooth_flash_runs_pending_casts_in_main_and_combat(self):
        for owner in (0, 1):
            direct = self.case('Tooth of Chiss-Goria', f'flash_response_{owner}')['steps']
            self.before(direct, 'announce_spell', 'cast_response', 'parent', 'source')
            self.assertIn(2, self.values(direct, 'assert_stack'))
            self.assertEqual(self.values(direct, 'set_mana', player=owner, color=0)[-1], 1)
            combat = self.case('Tooth of Chiss-Goria', f'flash_combat_response_{owner}')['steps']
            self.before(combat, 'require_can_cast', 'cast', 'source', 'parent')
            self.before(combat, 'announce_spell', 'cast_response', 'parent', 'source')
            self.before(combat, 'cast_response', 'activate', 'source', 'source')
            probe, = self.actions(combat, 'require_can_cast', ref='source', value=1)
            self.assertLess(next(i for i, step in enumerate(combat) if step['op'] == 'advance_combat'), combat.index(probe))
            self.assertEqual(self.values(combat, 'set_mana', player=owner, color=0)[-1], 1)
            self.assertEqual(self.values(combat, 'assert_mana', player=owner, color=0)[-1], 0)
            self.assertIn(2, self.values(combat, 'assert_stack'))
            self.assertFalse(self.actions(combat, 'choose_priority'))
            self.assertEqual(self.values(combat, 'assert_life', player=owner)[-1], 13)

    def test_tooth_pump_has_real_tap_and_expiry(self):
        steps = self.case('Tooth of Chiss-Goria', 'pump_attack_0')['steps']
        self.before(steps, 'activate', 'attack', 'source', 'ally')
        self.assertIn(1, self.values(steps, 'assert_card', ref='source', property='tapped'))
        self.assertIn(4, self.values(steps, 'assert_card', ref='ally', property='power'))
        self.assertEqual(self.values(steps, 'assert_card', ref='ally', property='power')[-1], 3)
        self.assertEqual(self.values(steps, 'assert_life', player=1)[-1], 16)
        denied = self.case('Tooth of Chiss-Goria', 'tapped_denied_1')['steps']
        self.assertEqual(self.values(denied, 'activate', ref='source'), [1, 0])

    def test_broodstar_tracks_actual_artifact_changes_and_zero_toughness(self):
        for owner in (0, 1):
            for mode, action, target in (('actual_artifact_entry', 'cast', 'changing'), ('control_changes_size', 'cast', 'steal')):
                steps = self.case('Broodstar', f'{mode}_{owner}')['steps']
                self.before(steps, 'cast', action, 'source', target)
                for axis in ('power', 'toughness'):
                    self.assertEqual(self.values(steps, 'assert_card', ref='source', property=axis), [1, 2])
            removed = self.case('Broodstar', f'last_artifact_leaves_{owner}')['steps']
            self.assertEqual(self.values(removed, 'assert_zone_multiplicity', player=owner, zone='graveyard', index=6629)[-1], 1)
            other = self.case('Broodstar', f'opponent_only_zero_{owner}')['steps']
            self.assertEqual(self.values(other, 'assert_zone_multiplicity', player=owner, zone='graveyard', index=6629)[-1], 1)

    def test_broodstar_flying_combat_uses_derived_size(self):
        steps = self.case('Broodstar', 'ground_block_denied_1')['steps']
        self.assertEqual(self.values(steps, 'block', ref='foe', other='source'), [0])
        self.assertEqual(self.values(steps, 'assert_life', player=0)[-1], 18)
        reach = self.case('Broodstar', 'reach_block_allowed_0')['steps']
        self.assertEqual(self.values(reach, 'block', ref='foe', other='source'), [1])
        self.assertEqual(self.values(reach, 'assert_card', ref='source', property='power')[-1], 4)
