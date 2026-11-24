import json
import unittest
from pathlib import Path

from generate_cases import registry
from optional_combat_damage_bypass_recipes import FAMILY, RULE, classify, expand, profiles, spec, validate_witnesses, variants


class OptionalCombatDamageBypassRecipesTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle = json.loads((Path(__file__).parent / 'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {row['name']: row for row in registry()}

    def declaration(self, name, selected):
        return dict(family=FAMILY, version=2, oracle_text=RULE, params=classify(self.oracle[name]), variants=selected)

    def steps(self, name, variant):
        declaration = self.declaration(name, [variant])
        params = spec(declaration, self.oracle[name])
        case, = expand(name, self.rows[name]['csvid'], declaration, params, [variant], self.rows, self.oracle)
        self.assertTrue(all('attacking' not in entry.get('state', []) for entry in case['setup']))
        return case['steps']

    def matching(self, steps, op, **fields):
        return [item for item in steps if item['op'] == op and all(item.get(key) == value for key, value in fields.items())]

    def values(self, steps, op, **fields):
        return [item['value'] for item in self.matching(steps, op, **fields)]

    def test_all_sources_have_exact_oracle_and_both_controller_matrices(self):
        validate_witnesses(self.oracle, self.rows)
        self.assertEqual({self.rows[name]['csvid'] for name in profiles()['sources']}, {3632, 1074, 4378, 1173, 4352})
        for name in profiles()['sources']:
            record = self.oracle[name]
            selected = variants(classify(record))
            declaration = self.declaration(name, selected)
            self.assertEqual(len(set(selected)), 30)
            self.assertEqual(sum(v.endswith('_1') for v in selected), 15)
            spec(declaration, record)
            for bad in (dict(declaration, version=2.0), dict(declaration, variants=[selected[0]] * 2),
                        dict(declaration, params=dict(declaration['params'], power=float(record['power'])))):
                with self.assertRaises(ValueError):
                    spec(bad, record)
            with self.assertRaises(ValueError):
                classify(dict(record, oracle_text=RULE + '\nTrample'))

    def test_choice_is_after_real_block_and_only_changes_outgoing_damage(self):
        for owner in (0, 1):
            bypass = self.steps('Pride of Lions', f'bypass_{owner}')
            order = [s['op'] for s in bypass]
            for left, right in zip(('attack', 'block', 'choose_dialog', 'advance_combat'),
                                   ('block', 'choose_dialog', 'advance_combat', 'combat_damage')):
                self.assertLess(order.index(left), order.index(right))
            self.assertEqual(self.values(bypass, 'choose_dialog', player=owner), [1])
            self.assertEqual(self.values(bypass, 'assert_card', ref='blocker', property='blocking_count'), [1, 1])
            self.assertEqual(self.values(bypass, 'assert_life', player=1 - owner)[-1], 16)
            self.assertEqual(self.values(bypass, 'assert_card', ref='source', property='damage')[-1], 2)
            self.assertEqual(self.values(bypass, 'assert_card', ref='blocker', property='damage')[-1], 0)
            normal = self.steps('Lone Wolf', f'normal_{owner}')
            self.assertEqual(self.values(normal, 'choose_dialog', player=owner), [0])
            self.assertEqual(self.values(normal, 'assert_life', player=1 - owner)[-1], 20)
            self.assertEqual(self.values(normal, 'assert_card', ref='blocker', property='damage')[-1], 2)
            self.assertEqual(self.values(normal, 'assert_zone_multiplicity', player=owner, zone='graveyard', index=1074)[-1], 1)

    def test_unblocked_defending_and_idle_have_no_choice(self):
        for mode, life in (('unblocked', 13), ('defending', 20), ('source_idle', 18)):
            steps = self.steps('Thorn Elemental', mode + '_1')
            self.assertFalse(self.matching(steps, 'choose_dialog'))
            self.assertEqual(self.values(steps, 'assert_life', player=0)[-1], life)
            self.assertEqual(self.values(steps, 'block', ref='source'), [1] if mode == 'defending' else [])

    def test_lethal_first_strike_and_unrelated_damage(self):
        lethal = self.steps('Lone Wolf', 'first_strike_0')
        self.assertEqual(self.values(lethal, 'choose_dialog', player=0), [1])
        self.assertEqual(self.values(lethal, 'assert_life', player=1)[-1], 20)
        self.assertEqual(self.values(lethal, 'assert_zone_multiplicity', zone='graveyard', player=0, index=1074)[-1], 1)
        separate = self.steps('Pride of Lions', 'mixed_attackers_1')
        self.assertEqual(self.values(separate, 'assert_life', player=0)[-1], 16)
        self.assertEqual(self.values(separate, 'assert_card', ref='second_blocker', property='damage')[-1], 2)

    def test_removed_blocker_retains_declared_bypass_contrast(self):
        for mode, life in (('bypass_removed_blocker', 13), ('normal_removed_blocker', 20)):
            steps = self.steps('Deathcoil Wurm', mode + '_0')
            ops = [s['op'] for s in steps]
            self.assertLess(ops.index('advance_combat'), ops.index('cast'))
            self.assertLess(ops.index('resolve'), ops.index('combat_damage'))
            self.assertEqual(self.values(steps, 'assert_zone_multiplicity', zone='hand', player=1, index=272)[-1], 1)
            self.assertEqual(self.values(steps, 'assert_life', player=1)[-1], life)

    def test_fight_damage_remains_on_creature(self):
        for owner in (0, 1):
            steps = self.steps('Wolf Pack', f'noncombat_when_chosen_{owner}')
            targets = self.matching(steps, 'choose_target', player=owner)
            clicks = self.matching(steps, 'choose_mana', player=owner)
            self.assertEqual([s['ref'] for s in targets], ['source', 'blocker'])
            self.assertEqual([s['color'] for s in clicks], [3] if owner == 0 else [])
            if clicks:
                self.assertLess(steps.index(clicks[0]), steps.index(targets[0]))
            self.assertEqual(self.values(steps, 'assert_card', ref='blocker', property='damage')[-2:], [7, 7])
            self.assertEqual(self.values(steps, 'assert_life', player=1 - owner)[-2:], [20, 13])

    def test_cast_uses_printed_cost_and_real_sickness_aging(self):
        cast = self.steps('Deathcoil Wurm', 'cast_then_attack_1')
        self.assertEqual(self.values(cast, 'set_mana', player=1, color=0), [0, 6])
        self.assertEqual(self.values(cast, 'set_mana', player=1, color=3), [0, 2])
        self.assertEqual(self.values(cast, 'attack', ref='source'), [0, 1])
        self.assertEqual([s['player'] for s in self.matching(cast, 'untap_step')], [0, 1])
        wrong = self.steps('Deathcoil Wurm', 'wrong_color_0')
        self.assertEqual(self.values(wrong, 'cast', ref='source'), [0])
        self.assertEqual(self.values(wrong, 'assert_mana', player=0, color=0)[-1], 7)
        self.assertEqual(self.values(wrong, 'assert_mana', player=0, color=3)[-1], 1)

    def test_replay_restores_choice_and_next_combat_can_choose_again(self):
        steps = self.steps('Thorn Elemental', 'choice_snapshot_1')
        ops = [s['op'] for s in steps]
        capture, record, restore = [ops.index(op) for op in ('capture', 'record_state', 'restore')]
        self.assertLess(ops.index('choose_dialog'), capture)
        self.assertEqual(steps[capture + 1:record], steps[restore + 1:-1])
        self.assertEqual(self.values(steps, 'assert_life', player=0)[-1], 13)
        repeat = self.steps('Thorn Elemental', 'rechoose_0')
        self.assertEqual(self.values(repeat, 'choose_dialog', player=0), [1, 0])
        self.assertEqual(len(self.matching(repeat, 'untap_step')), 2)
        self.assertEqual(self.values(repeat, 'assert_life', player=1)[-1], 13)
        self.assertEqual(self.values(repeat, 'assert_card', ref='blocker', property='damage')[-1], 7)
