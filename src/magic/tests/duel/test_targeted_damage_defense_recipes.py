import copy
import json
import unittest
from pathlib import Path

from generate_cases import registry
from targeted_damage_defense_recipes import FAMILY, classify, expand, spec, validate_witnesses, variants


class TargetedDamageDefenseRecipesTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle = json.loads((Path(__file__).parent / 'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {row['name']: row for row in registry()}

    def declaration(self, name):
        record = self.oracle[name]
        return dict(family=FAMILY, version=2, oracle_text=record['oracle_text'],
                    params=classify(record), variants=[])

    def steps(self, name, mode):
        declaration = self.declaration(name)
        params = spec(declaration, self.oracle[name])
        return expand(name, self.rows[name]['csvid'], declaration, params, [mode], self.rows, self.oracle)[0]['steps']

    def last(self, steps, op, **fields):
        return [s['value'] for s in steps if s['op'] == op and all(s.get(k) == v for k, v in fields.items())][-1]

    def test_complete_Oracle_parameters_distinguish_damage_roles_and_mana(self):
        maze = classify(self.oracle['Maze of Ith'])
        self.assertTrue(maze['untap'] and maze['incoming'] and maze['attacking_target'])
        self.assertFalse(maze['own_target'] or maze['mana_ability'])
        self.assertTrue(classify(self.oracle['Elvish Scout'])['own_target'])
        kor = classify(self.oracle['Kor Haven'])
        self.assertEqual(kor['activation_mana'], [1, 0, 0, 0, 0, 1])
        self.assertTrue(kor['mana_ability'] and kor['combat_only'])
        self.assertFalse(kor['incoming'] or kor['untap'])
        self.assertTrue(classify(self.oracle['Subdue'])['combat_only'])
        self.assertFalse(classify(self.oracle['Kry Shield'])['combat_only'])
        for name in ('Ith, High Arcanist', 'Dawnstrider', 'Prahv, Spires of Order', 'Sewers of Estark'):
            with self.assertRaises(ValueError):
                classify(self.oracle[name])

    def test_pins_and_JSON_types_fail_closed(self):
        name = 'Kor Haven'
        record = self.oracle[name]
        declaration = self.declaration(name)
        for key, value in [('untap', 0), ('mana_ability', 1), ('activation_mana', [1.0, 0, 0, 0, 0, 1])]:
            changed = copy.deepcopy(declaration)
            changed['params'][key] = value
            with self.assertRaises(ValueError):
                spec(changed, record)
        with self.assertRaises(ValueError):
            spec(dict(declaration, version=2.0), record)
        validate_witnesses(self.oracle, self.rows)
        mismatched = dict(self.oracle)
        mismatched['Craw Wurm'] = dict(self.oracle['Craw Wurm'], toughness='5')
        with self.assertRaises(ValueError):
            validate_witnesses(mismatched, self.rows)

    def test_maze_untaps_attacker_without_removing_it_and_leaves_other_damage(self):
        steps = self.steps('Maze of Ith', 'blocked_0')
        before_damage = next(i for i, s in enumerate(steps) if s['op'] == 'combat_damage')
        self.assertEqual(self.last(steps[:before_damage], 'assert_card', ref='selected', property='tapped'), 0)
        self.assertEqual(self.last(steps[:before_damage], 'assert_card', ref='selected', property='attacking'), 1)
        self.assertEqual(self.last(steps, 'assert_card', ref='selected', property='damage'), 0)
        self.assertEqual(self.last(steps, 'assert_card', ref='blocker', property='damage'), 0)
        self.assertEqual(self.last(steps, 'assert_life', player=0), 17)

    def test_subdue_preserves_noncombat_damage_while_kry_prevents_it(self):
        subdue = self.steps('Subdue', 'noncombat_outgoing_0')
        kry = self.steps('Kry Shield', 'noncombat_outgoing_1')
        for steps, expected_life in ((subdue, 16), (kry, 17)):
            self.assertTrue(any(s['op'] == 'activate' and s['ref'] == 'selected' for s in steps))
            self.assertEqual(self.last(steps, 'assert_life', player=0), expected_life)
        blocker = self.steps('Kry Shield', 'blocker_target_0')
        self.assertEqual(self.last(blocker, 'assert_card', ref='blocker', property='toughness'), 7)
        self.assertEqual(self.last(blocker, 'assert_card', ref='blocker', property='damage'), 6)

    def test_kor_menu_checks_actual_payment_and_separate_colorless_activation(self):
        exact = self.steps('Kor Haven', 'exact_payment_0')
        offered = next(i for i, s in enumerate(exact) if s['op'] == 'choose_dialog')
        self.assertEqual([step['op'] for step in exact[offered:offered+4]],
                         ['choose_dialog', 'choose_target', 'activate', 'assert_unused_choices'])
        self.assertEqual(self.last(exact[:offered], 'set_mana', player=0, color=0), 1)
        self.assertEqual(self.last(exact, 'assert_mana', player=0, color=0), 0)
        colorless = self.steps('Kor Haven', 'mana_1')
        self.assertEqual(self.last(colorless, 'assert_mana', player=1, color=0), 1)
        self.assertEqual(self.last(colorless, 'assert_stack'), 0)
        self.assertFalse(any(s['op'] in ('choose_dialog', 'choose_target') for s in colorless))

    def test_replay_captures_live_pending_effect_and_unaffected_cleanup_damage(self):
        waiting = self.steps('Elvish Scout', 'pending_snapshot_0')
        capture = next(i for i, step in enumerate(waiting) if step['op'] == 'capture')
        self.assertEqual(self.last(waiting[:capture], 'assert_stack'), 1)
        record = next(i for i, step in enumerate(waiting) if step['op'] == 'record_state')
        restored = next(i for i, step in enumerate(waiting) if step['op'] == 'restore')
        self.assertEqual(waiting[capture+1:record], waiting[restored+1:-1])
        cleanup = self.steps('Subdue', 'cleanup_0')
        self.assertEqual(sum(s['op'] == 'combat_damage' for s in cleanup), 2)
        self.assertEqual(self.last(cleanup, 'assert_card', ref='selected', property='toughness'), 4)
        self.assertEqual(self.last(cleanup, 'assert_life', player=0), 8)

    def test_kry_defense_chooses_own_blocker_for_both_controllers(self):
        name = 'Kry Shield'
        declaration = self.declaration(name)
        params = spec(declaration, self.oracle[name])
        cases = expand(name, 705, declaration, params, ['blocker_target_0', 'blocker_target_1'], self.rows, self.oracle)
        for owner, case in enumerate(cases):
            board = {card['ref']: card['player'] for card in case['setup']}
            self.assertEqual(board['source'], board['blocker'])
            self.assertEqual(board['selected'], 1 - owner)
            self.assertIn(dict(op='block', ref='blocker', other='selected', value=1), case['steps'])
            self.assertIn(dict(op='choose_target', player=owner, ref='blocker'), case['steps'])
            self.assertEqual(self.last(case['steps'], 'assert_card', ref='blocker', property='damage'), 6)
            self.assertEqual(self.last(case['steps'], 'assert_card', ref='selected', property='damage'), 0)

    def test_untap_defense_expiry_crosses_real_turns(self):
        for name in ('Maze of Ith', 'Elvish Scout'):
            for owner in (0, 1):
                steps = self.steps(name, f'cleanup_{owner}')
                actor = owner if name == 'Elvish Scout' else 1 - owner
                combats = [i for i, step in enumerate(steps) if step['op'] == 'combat_damage']
                self.assertEqual(len(combats), 2)
                between = steps[combats[0]:combats[1]]
                self.assertEqual(sum(step['op'] == 'advance_turn' for step in between), 2)
                self.assertEqual([s['player'] for s in between if s['op'] == 'untap_step'], [1-actor, actor])
                self.assertEqual(self.last(between, 'assert_life', player=1-actor), 17)
                self.assertEqual(self.last(steps, 'assert_life', player=1-actor), 8)
                self.assertEqual(self.last(steps, 'assert_card', ref='source', property='tapped'), 0)
                self.assertFalse(any(s.get('ref') == 'vitalize' for s in steps))

    def test_all_declared_variants_expand_for_both_controllers(self):
        counts = {'Maze of Ith': 26, 'Elvish Scout': 40, 'Kor Haven': 34, 'Subdue': 28, 'Kry Shield': 34}
        for name, expected in counts.items():
            declaration = self.declaration(name)
            params = spec(declaration, self.oracle[name])
            cases = expand(name, self.rows[name]['csvid'], declaration, params, variants(params), self.rows, self.oracle)
            self.assertEqual(len({case['name'] for case in cases}), expected)
            for case in cases:
                ops = {s['op'] for s in case['steps']}
                if 'combat_damage' in ops:
                    self.assertTrue({'begin_attackers', 'finish_attackers', 'advance_combat', 'end_combat'} <= ops)
