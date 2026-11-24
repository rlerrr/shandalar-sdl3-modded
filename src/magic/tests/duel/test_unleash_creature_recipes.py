import json
import unittest
from pathlib import Path

from generate_cases import registry
from recipes import COLORS
from unleash_creature_recipes import FAMILY, RULE, classify, expand, profiles, spec, validate_witnesses, variants


class UnleashCreatureRecipesTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle = json.loads((Path(__file__).parent / 'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {row['name']: row for row in registry()}

    def declaration(self, name, selected):
        return dict(family=FAMILY, version=2, oracle_text=RULE, params=classify(self.oracle[name]), variants=selected)

    def steps(self, name, selected):
        declaration = self.declaration(name, [selected])
        case, = expand(name, self.rows[name]['csvid'], declaration, spec(declaration, self.oracle[name]),
                       [selected], self.rows, self.oracle)
        self.assertTrue(all(not entry.get('state') for entry in case['setup']))
        return case['steps']

    def actions(self, steps, op, **fields):
        return [step for step in steps if step['op'] == op and all(step.get(k) == v for k, v in fields.items())]

    def values(self, steps, op, **fields):
        return [step['value'] for step in self.actions(steps, op, **fields)]

    def counters(self, steps, ref='source'):
        return self.values(steps, 'assert_card', ref=ref, property='plus1_counters')

    def test_pinned_full_oracle_and_fixed_typed_costs(self):
        validate_witnesses(self.oracle, self.rows)
        self.assertEqual({self.rows[name]['csvid'] for name in profiles()['sources']}, {12698, 12736, 12792, 12831})
        for name in profiles()['sources']:
            record = self.oracle[name]
            params = classify(record)
            selected = variants(params)
            self.assertEqual(len(set(selected)), 34 if name == 'Rakdos Cackler' else 32)
            declaration = self.declaration(name, selected)
            spec(declaration, record)
            with self.assertRaises(ValueError):
                spec(dict(declaration, version=2.0), record)
            with self.assertRaises(ValueError):
                spec(dict(declaration, params=dict(params, power=float(params['power']))), record)
            with self.assertRaises(ValueError):
                classify(dict(record, oracle_text='Trample\n' + RULE, keywords=['Trample', 'Unleash']))

    def test_paid_single_color_hybrid_and_unpayable_cast(self):
        for owner in (0, 1):
            for color, mode in (('B', 'pay_black'), ('R', 'pay_red')):
                steps = self.steps('Rakdos Cackler', f'{mode}_{owner}')
                for checked in ('B', 'R'):
                    amounts = self.values(steps, 'set_mana', player=owner, color=COLORS[checked])
                    self.assertEqual(amounts, [0, int(checked == color)])
                    self.assertEqual(self.values(steps, 'assert_mana', player=owner, color=COLORS[checked])[-1], 0)
                self.assertEqual(self.values(steps, 'choose_dialog', player=owner), [0])
                self.assertEqual(self.values(steps, 'cast', ref='source'), [1])
                self.assertEqual(self.counters(steps), [1])
            denied = self.steps('Rakdos Cackler', f'wrong_color_{owner}')
            self.assertEqual(self.values(denied, 'cast', ref='source'), [0])
            self.assertFalse(self.actions(denied, 'choose_dialog'))
            self.assertEqual(self.values(denied, 'assert_mana', player=owner, color=COLORS['U'])[-1], 1)

    def test_modal_accept_and_decline_change_real_combat(self):
        for owner in (0, 1):
            for accept in (False, True):
                steps = self.steps('Dead Reveler', f'block_{"accept" if accept else "decline"}_{owner}')
                self.assertEqual(self.values(steps, 'choose_dialog', player=owner), [int(not accept)])
                untap, = self.actions(steps, 'untap_step', player=1-owner)
                cast, = self.actions(steps, 'cast', ref='source')
                block, = self.actions(steps, 'block', ref='source', other='attacker')
                self.assertLess(steps.index(cast), steps.index(untap))
                self.assertLess(steps.index(untap), steps.index(block))
                self.assertEqual(block['value'], int(not accept))
                self.assertEqual(self.values(steps, 'assert_life', player=owner)[-1], 19 if accept else 20)
                self.assertEqual(self.values(steps, 'assert_zone_multiplicity', player=1-owner, zone='graveyard', index=163)[-1], int(not accept))

    def test_independent_counter_and_other_target_contrast(self):
        for mode, target, allowed in (('decline_external_counter', 'source', 0), ('grow_other', 'alternate', 1),
                                      ('double_counter', 'source', 0)):
            steps = self.steps('Spawn of Rix Maadi', mode + '_1')
            self.assertEqual(self.values(steps, 'choose_dialog', player=1), [int(mode != 'double_counter')])
            self.assertEqual(self.values(steps, 'cast', ref='growth'), [1])
            self.assertTrue(self.actions(steps, 'choose_target', player=1, ref=target))
            self.assertEqual(self.values(steps, 'block', ref='source', other='attacker'), [allowed])
            self.assertEqual(self.counters(steps, target)[-1], 2 if mode == 'double_counter' else 1)

    def test_real_sacrifice_clears_counter_and_later_block_is_legal(self):
        for owner in (0, 1):
            steps = self.steps('Gore-House Chainwalker', f'block_after_clear_{owner}')
            blocked, restored = self.actions(steps, 'block', ref='source')
            paid, = self.actions(steps, 'activate', ref='remover')
            self.assertEqual([blocked['value'], restored['value']], [0, 1])
            self.assertLess(steps.index(blocked), steps.index(paid))
            self.assertLess(steps.index(paid), steps.index(restored))
            self.assertEqual(self.values(steps, 'assert_zone_multiplicity', player=owner, zone='graveyard', index=10701)[-1], 1)
            self.assertEqual(self.counters(steps)[-1], 0)
            self.assertEqual(self.values(steps, 'assert_life', player=owner)[-1], 19)

    def test_real_noncast_entry_pays_for_piper_and_takes_both_choices(self):
        for decline in (False, True):
            steps = self.steps('Rakdos Cackler', f'piper_{"decline" if decline else "accept"}_0')
            self.assertEqual(self.values(steps, 'activate', ref='piper'), [1])
            self.assertFalse(self.actions(steps, 'cast', ref='source'))
            self.assertEqual(self.values(steps, 'choose_zone_card', player=0), [12792])
            self.assertEqual(self.values(steps, 'choose_dialog', player=0), [int(decline)])
            self.assertEqual(self.counters(steps)[0], int(not decline))
            self.assertEqual(self.values(steps, 'block', ref='source'), [int(decline)])

    def test_bounce_and_second_copy_take_separate_entry_choices(self):
        bounced = self.steps('Gore-House Chainwalker', 'bounce_rechoose_1')
        self.assertEqual(self.values(bounced, 'choose_dialog', player=1), [0, 1])
        self.assertEqual(self.values(bounced, 'cast', ref='bounce'), [1])
        self.assertEqual(self.values(bounced, 'bind_hand_copy', ref='source'), [12736])
        self.assertEqual(self.values(bounced, 'block', ref='source'), [1])
        separate = self.steps('Dead Reveler', 'separate_copies_0')
        self.assertEqual(self.counters(separate, 'source')[-1], 1)
        self.assertEqual(self.counters(separate, 'second')[-1], 0)
        self.assertEqual(self.values(separate, 'block', ref='source'), [0])
        self.assertEqual(self.values(separate, 'block', ref='second'), [1])

    def test_countered_source_retains_attack_permission_and_pending_replay(self):
        attack = self.steps('Spawn of Rix Maadi', 'boosted_attack_0')
        self.assertEqual(self.values(attack, 'attack', ref='source'), [1])
        self.assertEqual(self.counters(attack)[-1], 1)
        self.assertEqual(self.values(attack, 'assert_life', player=1)[-1], 14)
        replay = self.steps('Dead Reveler', 'pending_snapshot_1')
        ops = [step['op'] for step in replay]
        capture, record, restore = [ops.index(op) for op in ('capture', 'record_state', 'restore')]
        self.assertLess(ops.index('cast'), capture)
        self.assertEqual(ops[capture + 1], 'resolve')
        self.assertEqual(replay[capture + 1:record], replay[restore + 1:-1])
