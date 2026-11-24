"""Focused contracts for paid combat timing and exact Bloodrush model coverage."""
import copy
import json
import unittest
from pathlib import Path

import bloodrush_creature_recipes as recipe
from generate_cases import registry


class BloodrushRecipes(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        data = Path(__file__).parent / 'data'
        cls.oracle = json.loads((data / 'oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {row['name']: row for row in registry()}
        cls.profile = recipe.pins()
        recipe.validate_witnesses(cls.oracle, cls.rows)
        cls.cases = {}
        for name, pin in cls.profile['sources'].items():
            csvid = pin['csvid']
            path, = (data / 'cases/cards').glob(f'*/{csvid:05d}-*.json')
            declaration, = json.loads(path.read_text(encoding='utf8'))['recipes']
            model = recipe.spec(declaration, cls.oracle[name])
            variants = recipe.variants(model)
            if declaration['variants'] != variants:
                raise AssertionError(name + ' did not enroll the complete reviewed case matrix')
            cells = recipe.expand(name, csvid, declaration, model, variants, cls.rows, cls.oracle)
            cls.cases[name] = {cell['_recipe']['variant']: cell for cell in cells}

    def test_whole_printed_oracle_before_and_after_bloodrush(self):
        for name, pin in self.profile['sources'].items():
            with self.subTest(name=name):
                record = pin['oracle']
                self.assertEqual(record['oracle_text'].splitlines(), recipe.mechanism(record)['clauses'])
                for text in (
                        record['oracle_text'] + '\nWhenever this creature attacks, draw a card.',
                        record['oracle_text'].replace('attacking creature', 'creature you control'),
                        record['oracle_text'].replace('Discard this card', 'Discard a card'),
                        record['oracle_text'].replace('until end of turn', 'until your next turn')):
                    with self.assertRaises(ValueError):
                        recipe.mechanism(dict(record, oracle_text=text))

    def test_paid_hand_chooses_before_discard_and_resolution(self):
        for name, cells in self.cases.items():
            for player in (0, 1):
                for mode in ('paid_own', 'paid_opposing', 'post_block', 'pending_bounced', 'pending_replay'):
                    actions = cells[f'{mode}_{player}']['steps']
                    ops = [action['op'] for action in actions]
                    index = ops.index('activate_hand')
                    self.assertLess(ops.index('finish_attackers'), index)
                    self.assertEqual(actions[index-3:index], [
                        dict(op='require_can_activate_hand', ref='source', value=1),
                        dict(op='choose_dialog', player=player, value=1),
                        dict(op='choose_target', ref='attacker', player=player)])
                    first_resolve = ops.index('resolve')
                    paid = actions[index:first_resolve]
                    self.assertIn(dict(op='assert_stack', value=1), paid)
                    self.assertIn(dict(op='assert_zone_multiplicity', player=player, zone='graveyard',
                                       index=self.rows[name]['csvid'], value=1), paid)
                    self.assertIn(dict(op='assert_zone_multiplicity', player=player, zone='hand',
                                       index=self.rows['Forest']['csvid'], value=1), paid)
                    self.assertTrue(all(s['value'] == 0 for s in paid if s['op'] == 'assert_mana'))
                    target = next(item for item in cells[f'{mode}_{player}']['setup'] if item['ref'] == 'attacker')
                    self.assertEqual(target['player'], 1-player if mode == 'paid_opposing' else player)
                    self.assertNotIn('state', target)

    def test_unaffordable_ineligible_and_declined_branches_do_not_pay(self):
        denied = ('main_no_attack', 'combat_no_attack', 'hand_short', 'hand_wrong_color', 'activation_cancel')
        for name, cells in self.cases.items():
            for player in (0, 1):
                for mode in denied:
                    with self.subTest(name=name, player=player, mode=mode):
                        actions = cells[f'{mode}_{player}']['steps']
                        self.assertIn(dict(op='activate_hand', ref='source', value=0), actions)
                        self.assertIn(dict(op='require_can_activate_hand', ref='source',
                                           value=int(mode == 'activation_cancel')), actions)
                        self.assertIn(dict(op='assert_zone_multiplicity', player=player, zone='hand',
                                           index=self.rows[name]['csvid'], value=1), actions)
                        self.assertFalse(any(action['op'] == 'choose_target' for action in actions))
                        self.assertEqual([action['value'] for action in actions if action['op'] == 'choose_dialog'],
                                         [0] if mode == 'activation_cancel' else [])

    def test_post_block_combat_lifecycle_and_expiry_for_every_case(self):
        for name, cells in self.cases.items():
            for label, cell in cells.items():
                with self.subTest(name=name, label=label):
                    ops = [step['op'] for step in cell['steps']]
                    self.assertEqual(ops.count('advance_combat'), ops.count('combat_damage'))
                    self.assertEqual(ops.count('combat_damage'), ops.count('end_combat'))
                    stages = [op for op in ops if op in ('advance_combat', 'combat_damage', 'end_combat')]
                    self.assertEqual(stages, ['advance_combat', 'combat_damage', 'end_combat'] * ops.count('end_combat'))
                    if label.startswith('post_block_'):
                        self.assertLess(ops.index('advance_combat'), ops.index('activate_hand'))
                        self.assertLess(ops.index('resolve'), ops.index('combat_damage'))
                    if label.startswith('keyword_expires_'):
                        self.assertLess(ops.index('cleanup'), ops.index('untap_step'))
                        self.assertEqual(ops.count('advance_turn'), 2)
                        self.assertEqual(ops.count('activate_hand'), 1)
                        self.assertEqual(ops.count('combat_damage'), 2)

    def test_pending_bounce_and_paid_state_replay(self):
        for cells in self.cases.values():
            for player in (0, 1):
                bounce = cells[f'pending_bounced_{player}']['steps']
                self.assertLess(bounce.index(dict(op='activate_hand', ref='source', value=1)),
                                bounce.index(dict(op='cast', ref='response', value=1)))
                self.assertLess(bounce.index(dict(op='assert_stack', value=2)), bounce.index(dict(op='resolve')))
                self.assertIn(dict(op='assert_card', ref='attacker', property='in_play', value=0), bounce)
                replay = cells[f'pending_replay_{player}']['steps']
                ops = [step['op'] for step in replay]
                self.assertEqual(ops.count('activate_hand'), 1)
                self.assertEqual(ops.count('resolve'), 2)
                self.assertLess(ops.index('activate_hand'), ops.index('capture'))
                self.assertEqual(replay[ops.index('capture')+1:ops.index('record_state')],
                                 replay[ops.index('ai_roundtrip')+2:ops.index('assert_recorded_state')])

    def test_witness_and_source_pins_are_independent(self):
        oracle = copy.copy(self.oracle)
        oracle['Grizzly Bears'] = dict(oracle['Grizzly Bears'], power='3')
        with self.assertRaisesRegex(ValueError, 'Grizzly Bears'):
            recipe.validate_witnesses(oracle, self.rows)
        rows = copy.copy(self.rows)
        rows['Wasteland Viper'] = dict(rows['Wasteland Viper'], handler='card_dummy')
        with self.assertRaisesRegex(ValueError, 'Wasteland Viper'):
            recipe.validate_witnesses(self.oracle, rows)

    def combat_receipts(self, name, label, player):
        actions = self.cases[name][f'{label}_{player}']['steps']
        end = max(i for i, action in enumerate(actions) if action['op'] == 'end_combat')
        return actions[end+1:]

    def assert_moved_to_graveyard(self, actions, name, player):
        self.assertIn(dict(op='assert_zone_multiplicity', player=player, zone='graveyard',
                           index=self.rows[name]['csvid'], value=1), actions)

    def test_first_strike_preempts_lethal_blocker_and_expires(self):
        name = 'Viashino Shanktail'
        for player in (0, 1):
            for mode, ref in (('post_block', 'attacker'), ('printed_keyword', 'source')):
                actions = self.combat_receipts(name, mode, player)
                self.assertIn(dict(op='assert_card', ref=ref, property='in_play', value=1), actions)
                self.assertIn(dict(op='assert_card', ref=ref, property='damage', value=0), actions)
                self.assert_moved_to_graveyard(actions, 'Hill Giant', 1-player)
            expired = self.combat_receipts(name, 'keyword_expires', player)
            self.assert_moved_to_graveyard(expired, 'Grizzly Bears', player)
            self.assert_moved_to_graveyard(expired, 'Goblin Piker', 1-player)

    def test_double_strike_hits_twice_and_keeps_a_dead_blocker_blocked(self):
        name = 'Wrecking Ogre'
        for player in (0, 1):
            for mode, life in (('paid_own', 10), ('printed_double_hit', 14), ('double_blocker_gone', 20)):
                actions = self.combat_receipts(name, mode, player)
                self.assertIn(dict(op='assert_life', player=1-player, value=life), actions)
            self.assert_moved_to_graveyard(self.combat_receipts(name, 'double_blocker_gone', player),
                                           'Hill Giant', 1-player)
            self.assert_moved_to_graveyard(self.combat_receipts(name, 'post_block', player),
                                           'Dazzling Ramparts', 1-player)
            for mode, damage in (('printed_keyword', 6), ('keyword_expires', 2)):
                self.assertIn(dict(op='assert_card', ref='blocker', property='damage', value=damage),
                              self.combat_receipts(name, mode, player))


if __name__ == '__main__':
    unittest.main()
