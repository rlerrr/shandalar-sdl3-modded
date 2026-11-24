"""Guard pending targets, legal phase windows, exact payments and placements."""
import json
import unittest

import pending_spell_composition_recipes as model
from generate_cases import registry


class PendingSpellCompositionTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle = json.loads((model.PROFILE.parent / 'oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {row['name']: row for row in registry()}

    def steps(self, card, variant):
        record = self.oracle[card]
        params = model.parameters(record)
        declaration = dict(family=model.FAMILY, version=2, params=params,
                           oracle_text=record['oracle_text'], variants=[variant])
        return model.expand(card, self.rows[card]['csvid'], declaration, model.spec(declaration, record),
                            [variant], self.rows, self.oracle)[0]['steps']

    @staticmethod
    def values(steps, op, **fields):
        return [s['value'] for s in steps if s['op'] == op and all(s.get(k) == v for k, v in fields.items())]

    def test_whole_oracle_and_action_witnesses(self):
        model.validate_witnesses(self.oracle, self.rows)
        self.assertEqual({pin['registry']['csvid'] for pin in model.profiles()['sources'].values()},
                         {9249, 9268, 9450})
        for card, pin in model.profiles()['sources'].items():
            record = self.oracle[card]
            self.assertEqual(model.parameters(record), pin['params'])
            with self.assertRaises(ValueError):
                model.parameters(dict(record, oracle_text=record['oracle_text'] + '\nDraw a card.'))

    def test_clash_counter_delays_mana_to_the_right_main_boundary(self):
        for owner in (0, 1):
            for branch, mana, boundary in (('win', 1, 'advance_turn'), ('tie', 0, 'advance_turn'),
                                            ('own_spell', 3, 'end_combat'), ('x_value', 4, 'advance_turn'),
                                            ('optional_decline', 0, 'advance_turn')):
                steps = self.steps('Scattering Stroke', f'{branch}_{owner}')
                which = [i for i, step in enumerate(steps) if step['op'] == boundary][-1]
                self.assertEqual(self.values(steps[:which], 'assert_mana', player=owner, color=0)[-1], 0)
                self.assertEqual(self.values(steps[which:], 'assert_mana', player=owner, color=0)[-1], mana)
                self.assertIn(dict(op='resolve_response'), steps[:which])
                if branch in ('win', 'own_spell', 'x_value', 'optional_decline'):
                    self.assertEqual(steps[which-1], dict(op='choose_dialog', player=owner, value=int(branch == 'optional_decline')))
            unbeaten = self.steps('Scattering Stroke', f'uncounterable_{owner}')
            self.assertEqual(self.values(unbeaten, 'assert_card', ref='parent', property='in_play'), [1])

    def test_own_enchantment_can_be_chosen_while_only_opponent_sweep_follows(self):
        for owner in (0, 1):
            won = self.steps('Spring Cleaning', f'own_target_{owner}')
            self.assertIn(dict(op='choose_target', player=owner, ref='target'), won)
            self.assertEqual(self.values(won, 'assert_card', ref='our_enchantment', property='in_play'), [1])
            for ref in ('target', 'their_enchantment', 'their_other'):
                self.assertEqual(self.values(won, 'assert_card', ref=ref, property='in_play'), [0])
            tied = self.steps('Spring Cleaning', f'tie_{owner}')
            self.assertEqual(self.values(tied, 'assert_card', ref='their_other', property='in_play'), [1])
            fizzle = self.steps('Spring Cleaning', f'fizzle_{owner}')
            self.assertEqual(self.values(fizzle, 'assert_card', ref='their_other', property='in_play'), [1])
            self.assertEqual(fizzle[fizzle.index(dict(op='resolve_response'))+1], dict(op='assert_unused_choices', value=4))

    def test_x_damage_targets_and_first_legal_recast(self):
        card = "Titan's Revenge"
        for owner in (0, 1):
            zero = self.steps(card, f'x_zero_{owner}')
            self.assertEqual(self.values(zero, 'choose_x', player=owner), [0])
            self.assertEqual(self.values(zero, 'assert_life', player=1-owner), [20])
            self.assertEqual(len(self.values(zero, 'bind_hand', ref='source')), 1)
            recast = self.steps(card, f'recast_{owner}')
            self.assertEqual(self.values(recast, 'choose_x', player=owner), [3, 2])
            self.assertEqual(self.values(recast, 'cast', ref='source'), [1, 1])
            self.assertEqual(self.values(recast, 'assert_card', ref='target', property='damage'), [3, 3])
            self.assertEqual(self.values(recast, 'assert_life', player=1-owner), [20, 18])
            walker = self.steps(card, f'walker_{owner}')
            self.assertEqual(self.values(walker, 'assert_counter', ref='target', counter_kind='loyalty'), [1])

    def test_replay_uses_actual_pending_parent_and_real_human_bottom(self):
        for name in model.profiles()['sources']:
            steps = self.steps(name, 'snapshot_1')
            self.assertEqual(sum(s['op'] == 'capture' for s in steps), 1)
            self.assertEqual(steps[-1], dict(op='assert_recorded_state'))
        bottom = self.steps("Titan's Revenge", 'human_bottom_1')
        self.assertEqual(self.values(bottom, 'choose_zone_card', player=0), [126])
        self.assertEqual(self.values(bottom, 'choose_zone', player=1), [-1])
