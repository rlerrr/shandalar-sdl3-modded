"""Focused independent checks for additional block declaration and combat receipts."""

import copy
import json
import unittest
from pathlib import Path

import additional_blocker_recipes as model
from generate_cases import registry


DUEL = Path(__file__).parent


class AdditionalBlockerRecipes(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle = json.loads((DUEL / 'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {row['name']: row for row in registry()}
        cls.homes = {}
        for name in model.profile()['sources']:
            csvid = cls.rows[name]['csvid']
            home, = (DUEL / f'data/cases/cards/{csvid // 100:03d}').glob(f'{csvid:05d}-*.json')
            cls.homes[name] = json.loads(home.read_text(encoding='utf8'))

    def build(self, mode, player=0, name='Foriysian Brigade'):
        home = self.homes[name]
        recipe, = home['recipes']
        params = model.spec(recipe, self.oracle[name])
        case, = model.expand(name, home['csvid'], recipe, params, [f'{mode}_{player}'], self.rows, self.oracle)
        return case

    def first_run(self, mode, player=0, name='Foriysian Brigade'):
        steps = self.build(mode, player, name)['steps']
        return steps[:next(i for i, row in enumerate(steps) if row['op'] == 'record_state')]

    @staticmethod
    def blocks(steps):
        return [(row['ref'], row['other'], row['value']) for row in steps if row['op'] == 'block']

    def test_all_sources_and_witnesses_match_exact_oracle_and_registry(self):
        model.validate_witnesses(self.oracle, self.rows)
        self.assertEqual({home['csvid'] for home in self.homes.values()}, {1397, 4208, 7953, 10453, 16019, 16759, 17542, 18927})
        for name, home in self.homes.items():
            recipe, = home['recipes']
            self.assertEqual(recipe['oracle_text'], self.oracle[name]['oracle_text'])
            self.assertEqual(recipe['variants'], model.variants(model.spec(recipe, self.oracle[name])))

    def test_grammar_rejects_missing_passive_wrong_extra_count_and_changed_cost(self):
        rec = self.oracle['Selesnya Sagittars']
        decl, = self.homes[rec['name']]['recipes']
        for update in (dict(keywords=[]), dict(oracle_text=model.CLAUSE + '\nFlying')):
            with self.assertRaises(ValueError):
                model.classify(rec | update)
        for key, value in (('additional', 2), ('reach', False), ('cost', [5, 0, 0, 0, 0, 0])):
            changed = copy.deepcopy(decl)
            changed['params'][key] = value
            with self.assertRaises(ValueError):
                model.spec(changed, rec)

    def test_second_real_block_has_distinct_target_and_shadow_stays_invisible(self):
        for who in (0, 1):
            with self.subTest(who=who):
                run = self.first_run('two_ground', who)
                self.assertEqual(self.blocks(run), [('source', 'first', 1), ('source', 'second', 1)])
                self.assertIn(dict(op='assert_card', ref='source', property='blocking_count', value=2), run)
                for target in ('first', 'second'):
                    self.assertIn(dict(op='assert_card', ref=target, property='blocked', value=1), run)
                self.assertTrue(all(row['value'] == 1 for row in run if row['op'] == 'assert_zone'
                                    and row['player'] == who and row['zone'] == 'battlefield'))

    def test_limit_duplicate_tapped_and_ordinary_contrasts(self):
        self.assertEqual(self.blocks(self.first_run('third_rejected'))[-1], ('source', 'third', 0))
        self.assertIn(dict(op='assert_life', player=0, value=17), self.first_run('third_rejected'))
        self.assertEqual(self.blocks(self.first_run('duplicate_rejected')),
                         [('source', 'first', 1), ('source', 'first', 0), ('source', 'second', 1)])
        self.assertEqual(self.blocks(self.first_run('ordinary_limit')), [('ordinary', 'first', 1), ('ordinary', 'second', 0)])
        self.assertEqual(self.blocks(self.first_run('tapped')), [('source', 'first', 0), ('source', 'second', 0)])

    def test_combined_receipt_with_both_incoming_hits_and_lethal_variant(self):
        run = self.first_run('two_ground')
        self.assertIn(dict(op='assert_card', ref='source', property='damage', value=2), run)
        self.assertIn(dict(op='assert_life', player=0, value=20), run)
        night = self.first_run('two_ground', 1, 'Night Market Guard')
        self.assertIn(dict(op='assert_zone_multiplicity', player=1, zone='graveyard', index=16759, value=1), night)
        self.assertNotIn(dict(op='assert_card', ref='source', property='damage', value=2), night)
        single = self.first_run('one_damage')
        self.assertIn(dict(op='assert_card', ref='first', property='damage', value=2), single)
        self.assertIn(dict(op='assert_card', ref='source', property='damage', value=1), single)
        self.assertIn(dict(op='assert_life', player=0, value=19), single)

    def test_reach_is_used_on_original_and_additional_assignments(self):
        for who in (0, 1):
            self.assertEqual(self.blocks(self.first_run('flyer_second', who))[-1], ('source', 'second', 0))
            for mode in ('flyer_first', 'flyer_second', 'two_flyers'):
                self.assertEqual(self.blocks(self.first_run(mode, who, 'Selesnya Sagittars')),
                                 [('source', 'first', 1), ('source', 'second', 1)])

    def test_trample_spills_only_excess_to_the_defending_seat(self):
        for owner in (0, 1):
            for mode, life in [('trample_spill', 15), ('trample_no_excess', 20), ('trample_unblocked', 13)]:
                run = self.first_run(mode, owner, 'Ghastbark Twins')
                last_life = next(row for row in reversed(run) if row['op'] == 'assert_life' and row['player'] == 1 - owner)
                self.assertEqual(last_life['value'], life)
            run = self.first_run('one_damage', owner, 'Ghastbark Twins')
            self.assertTrue(all(row['value'] == 20 for row in run if row['op'] == 'assert_life' and row['player'] == 1 - owner))

    def test_actual_cast_pays_then_survives_into_a_block_during_sickness(self):
        for owner in (0, 1):
            case = self.build('cast_then_block', owner, 'Selesnya Sagittars')
            run = case['steps']
            fee = case['_recipe']['params']['cost']
            self.assertEqual(fee, [3, 0, 0, 1, 0, 1])
            cast = run.index(dict(op='cast', ref='source', value=1))
            self.assertTrue(all(dict(op='set_mana', player=owner, color=c, value=n) in run[:cast] for c, n in enumerate(fee)))
            paid = run.index(dict(op='resolve'), cast)
            sick = run.index(dict(op='assert_card', ref='source', property='sick', value=1))
            blocked = run.index(dict(op='block', ref='source', other='first', value=1))
            self.assertTrue(cast < paid < sick < blocked)
            self.assertIn(dict(op='untap_step', player=1 - owner), run[paid:sick])

    def test_new_combat_and_post_blocks_snapshot(self):
        repeat = self.first_run('next_combat')
        self.assertEqual(self.blocks(repeat), [('source', 'first', 1), ('source', 'second', 1)] * 2)
        self.assertEqual([row['op'] for row in repeat].count('advance_turn'), 2)
        self.assertIn(dict(op='cast', ref='fog', value=1), repeat)
        steps = self.build('after_blocks_replay')['steps']
        ops = [row['op'] for row in steps]
        self.assertLess(ops.index('advance_combat'), ops.index('capture'))
        self.assertLess(ops.index('capture'), ops.index('combat_damage'))
        self.assertEqual(ops.count('advance_combat'), 1)
        self.assertEqual(ops.count('combat_damage'), 2)


    def test_human_assigns_each_point_to_real_blocked_attackers(self):
        for name, mode, targets in (
            ('Foriysian Brigade', 'two_ground', ['first'] * 2),
            ('Foriysian Brigade', 'reverse', ['first'] * 2),
            ('Selesnya Sagittars', 'flyer_second', ['first'] * 2),
            ('Spike-Tailed Ceratops', 'third_rejected', ['first'] * 4),
            ('Night Market Guard', 'two_ground', ['first'] * 3),
        ):
            with self.subTest(name=name):
                human = self.first_run(mode, 0, name)
                damage = human.index(dict(op='combat_damage'))
                choices = [dict(op='choose_target', ref=target, player=0) for target in targets]
                self.assertEqual(human[damage - len(choices):damage], choices)
                self.assertFalse(any(row['op'] == 'choose_target' for row in self.first_run(mode, 1, name)))
        self.assertFalse(any(row['op'] == 'choose_target' for row in self.first_run('flyer_second', 0)))

    def test_combat_assignment_count_clears_at_actual_cleanup(self):
        zero = dict(op='assert_card', ref='source', property='blocking_count', value=0)
        for mode in ('two_ground', 'one_damage', 'flyer_second'):
            run = self.first_run(mode, 1)
            after_combat = run.index(dict(op='end_combat')) + 1
            self.assertEqual(run[after_combat:after_combat + 2], [dict(op='cleanup'), zero])
        no_assignment = self.first_run('leave_unblocked', 1)
        after_combat = no_assignment.index(dict(op='end_combat')) + 1
        self.assertEqual(no_assignment[after_combat:after_combat + 2], [zero, dict(op='cleanup')])


    def test_fixed_eight_unlimited_and_defender_controls(self):
        for owner in (0, 1):
            for name in ('Palace Guard', 'Wall of Glare'):
                run = self.first_run('unbounded_nine', owner, name)
                declarations = self.blocks(run)
                self.assertEqual(len({entry[1] for entry in declarations}), 9)
                self.assertEqual([entry[2] for entry in declarations], [1] * 9)
                self.assertIn(dict(op='assert_card', ref='source', property='blocking_count', value=9), run)
            run = self.first_run('ninth_rejected', owner, 'Watcher in the Web')
            self.assertEqual([entry[2] for entry in self.blocks(run)], [1] * 8 + [0])
            self.assertIn(dict(op='assert_card', ref='attacker_8', property='blocked', value=1), run)
            self.assertIn(dict(op='assert_card', ref='attacker_9', property='blocked', value=0), run)
            defender = self.first_run('defender_rejected', owner, 'Wall of Glare')
            self.assertIn(dict(op='attack', ref='source', value=0), defender)
            self.assertIn(dict(op='attack', ref='ordinary', value=1), defender)


if __name__ == '__main__':
    unittest.main()
