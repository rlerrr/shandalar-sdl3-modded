import copy
import json
import unittest
from pathlib import Path

from generate_cases import registry
import cycle_discard_self_reward_recipes as recipe


class CycleDiscardRewardTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle = json.loads((Path(__file__).parent / 'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {row['name']: row for row in registry()}

    def case(self, variant='own_cycle_0', name='Hekma Sentinels'):
        record = self.oracle[name]
        declaration = dict(family=recipe.FAMILY, version=2, oracle_text=record['oracle_text'],
                           params=recipe.classify(record), variants=[variant])
        return recipe.expand(name, self.rows[name]['csvid'], declaration, recipe.spec(declaration, record),
                             [variant], self.rows, self.oracle)[0]

    def value(self, case, operation, **fields):
        return next(step['value'] for step in reversed(case['steps']) if step['op'] == operation
                    and all(step.get(key) == value for key, value in fields.items()))

    def stats(self, case, ref='source'):
        return tuple(self.value(case, 'assert_card', ref=ref, property=field) for field in ('power', 'toughness'))

    def copies(self, case, name, who, zone):
        return self.value(case, 'assert_zone_multiplicity', player=who, zone=zone, index=self.rows[name]['csvid'])

    def test_pinned_whole_cohort_and_real_handlers(self):
        recipe.validate_witnesses(self.oracle, self.rows)
        self.assertEqual(set(recipe.pins()['sources']), {'Hekma Sentinels', 'Flameblade Adept',
                         'Horror of the Broken Lands', 'Pitiless Vizier', 'Cunning Survivor', 'Grisly Survivor'})
        rows = copy.deepcopy(self.rows)
        rows['Hekma Sentinels']['handler'] = 'card_dummy'
        with self.assertRaises(ValueError):
            recipe.validate_witnesses(self.oracle, rows)

    def test_extra_clause_and_controller_drift_rejected(self):
        record = self.oracle['Hekma Sentinels']
        for text in (record['oracle_text'] + '\nFlying', record['oracle_text'].replace('you cycle', 'an opponent cycles'),
                     record['oracle_text'].replace('until end of turn', 'until your next turn')):
            with self.assertRaises(ValueError):
                recipe.classify(dict(record, oracle_text=text))

    def test_hand_cycle_has_real_cost_and_distinct_draw_resolution(self):
        for who in (0, 1):
            case = self.case(f'own_cycle_{who}')
            steps = case['steps']
            activation = next(i for i, step in enumerate(steps) if step['op'] == 'activate_hand')
            resolution = next(i for i, step in enumerate(steps) if step['op'] == 'resolve')
            self.assertIn(dict(op='choose_dialog', player=who, value=1), steps[:activation])
            self.assertIn(dict(op='assert_stack', value=1), steps[activation:resolution])
            self.assertIn(dict(op='assert_zone', player=who, zone='library', value=4), steps[activation:resolution])
            self.assertEqual(self.copies(case, 'Expunge', who, 'graveyard'), 1)
            self.assertEqual(self.copies(case, 'Grizzly Bears', who, 'hand'), 1)
            self.assertEqual(self.value(case, 'assert_mana', player=who, color=0), 0)
            self.assertEqual(self.stats(case), (3, 4))

    def test_plain_discard_pays_tireless_tribe_and_never_draws(self):
        for who in (0, 1):
            case = self.case(f'own_discard_{who}')
            self.assertIn(dict(op='activate', ref='outlet', value=1), case['steps'])
            self.assertFalse(any(step['op'] == 'activate_hand' for step in case['steps']))
            self.assertEqual(self.stats(case, 'outlet'), (1, 5))
            self.assertEqual(self.stats(case), (3, 4))
            self.assertEqual(self.copies(case, 'Grizzly Bears', who, 'graveyard'), 1)
            self.assertEqual(self.value(case, 'assert_zone', player=who, zone='library'), 4)

    def test_opponent_cycle_discard_and_draw_do_not_reward_source(self):
        for mode in ('opponent_cycle', 'opponent_discard', 'draw_only'):
            case = self.case(mode + '_0')
            self.assertEqual(self.stats(case), (2, 3))
        self.assertEqual(self.copies(self.case('opponent_cycle_0'), 'Expunge', 1, 'graveyard'), 1)
        self.assertEqual(self.copies(self.case('draw_only_0'), 'Grizzly Bears', 0, 'hand'), 1)

    def test_repeated_and_mixed_actions_reward_once_per_card(self):
        repeated = self.case('repeated_cycle_1')
        mixed = self.case('cycle_then_discard_1')
        self.assertEqual(self.stats(repeated), (4, 5))
        self.assertEqual(self.stats(mixed), (4, 5))
        self.assertEqual(self.copies(repeated, 'Expunge', 1, 'graveyard'), 2)
        self.assertEqual(self.copies(mixed, 'Grizzly Bears', 1, 'graveyard'), 1)
        self.assertEqual(self.value(mixed, 'assert_zone', player=1, zone='library'), 3)
        self.assertEqual(self.stats(self.case('repeated_cycle_1', 'Grisly Survivor')), (6, 3))

    def test_cancel_and_unaffordable_preserve_card_and_reward_absence(self):
        for mode in ('cycle_cancel', 'cycle_unaffordable'):
            case = self.case(mode + '_0')
            self.assertEqual(self.stats(case), (2, 3))
            self.assertEqual(self.copies(case, 'Expunge', 0, 'hand'), 1)
            self.assertEqual(self.copies(case, 'Expunge', 0, 'graveyard'), 0)
        self.assertFalse(any(step['op'] == 'choose_dialog' for step in self.case('cycle_unaffordable_0')['steps']))

    def test_cleanup_expires_real_stat_reward(self):
        case = self.case('expiry_0')
        self.assertIn(dict(op='assert_card', ref='source', property='power', value=3), case['steps'])
        self.assertEqual(self.stats(case), (2, 3))

    def test_menace_and_temporary_unblockability_use_real_block_declarations(self):
        single = self.case('menace_single_0', 'Flameblade Adept')
        pair = self.case('menace_pair_0', 'Flameblade Adept')
        self.assertEqual(self.value(single, 'block', ref='blocker'), 0)
        self.assertEqual(self.value(pair, 'block', ref='second_blocker'), 1)
        self.assertEqual(self.value(single, 'assert_life', player=1), 18)
        for mode, legal in [('unblockable', 0), ('no_reward_block', 1), ('expired_block', 1)]:
            case = self.case(mode + '_1', 'Cunning Survivor')
            self.assertEqual(self.value(case, 'block', ref='blocker'), legal)
            self.assertEqual(self.value(case, 'assert_life', player=0), 18 if not legal else 20)

    def test_expired_block_declares_combat_on_the_next_own_first_main(self):
        for owner in (0, 1):
            case = self.case(f'expired_block_{owner}', 'Cunning Survivor')
            steps = case['steps']
            attack = next(i for i, step in enumerate(steps) if step['op'] == 'begin_attackers')
            self.assertEqual(steps[attack-6:attack], [dict(op='advance_turn'), dict(op='untap_step', player=1-owner),
                dict(op='advance_turn'), dict(op='untap_step', player=owner), dict(op='turn', player=owner, value=0), dict(op='refresh')])

    def test_indestructibility_protects_only_while_active_and_not_from_zero_toughness(self):
        name = 'Pitiless Vizier'
        for mode, survives in [('survives_destroy', True), ('base_destroy', False),
                               ('expired_destroy', False), ('zero_toughness', False)]:
            case = self.case(mode + '_0', name)
            self.assertEqual(self.copies(case, name, 0, 'battlefield'), int(survives))
            self.assertEqual(self.copies(case, name, 0, 'graveyard'), int(not survives))
        self.assertEqual(self.copies(self.case('zero_toughness_0', name), 'Grasp of Darkness', 1, 'graveyard'), 1)

    def test_horror_own_cycling_and_another_actual_copy(self):
        name = 'Horror of the Broken Lands'
        own = self.case('self_cycle_0', name)
        second = self.case('second_copy_cycles_1', name)
        self.assertEqual(self.copies(own, name, 0, 'graveyard'), 1)
        self.assertEqual(self.copies(own, name, 0, 'battlefield'), 0)
        self.assertEqual(self.copies(second, name, 1, 'graveyard'), 1)
        self.assertEqual(self.stats(second), (6, 5))

    def test_pending_replay_repeats_draw_only_after_actual_cycle_activation(self):
        case = self.case('pending_replay_1')
        steps = case['steps']
        capture, record, restore = [next(i for i, step in enumerate(steps) if step['op'] == op)
                                    for op in ('capture', 'record_state', 'restore')]
        self.assertTrue(any(step['op'] == 'activate_hand' for step in steps[:capture]))
        self.assertEqual(steps[capture+1:record], steps[restore+1:-1])
        self.assertFalse(any(step['op'] == 'activate_hand' for step in steps[restore+1:]))


if __name__ == '__main__':
    unittest.main()
