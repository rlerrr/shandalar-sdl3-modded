"""Focused checks for whole-Oracle paid attack declarations."""

import copy
import json
import unittest
from pathlib import Path

import global_attack_tax_recipes as model
from generate_cases import registry


DUEL = Path(__file__).parent


class GlobalAttackTaxRecipes(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle = json.loads((DUEL / 'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {row['name']: row for row in registry()}
        cls.homes = {}
        for name in model.profile()['sources']:
            csvid = cls.rows[name]['csvid']
            home, = (DUEL / f'data/cases/cards/{csvid // 100:03d}').glob(f'{csvid:05d}-*.json')
            cls.homes[name] = json.loads(home.read_text(encoding='utf8'))

    def first_run(self, name, variant):
        home = self.homes[name]
        recipe, = home['recipes']
        params = model.spec(recipe, self.oracle[name])
        case, = model.expand(name, home['csvid'], recipe, params, [variant], self.rows, self.oracle)
        steps = case['steps']
        return steps[:steps.index(dict(op='record_state'))]

    @staticmethod
    def declarations(run):
        start = run.index(dict(op='begin_attackers'))
        end = run.index(dict(op='finish_attackers'))
        return run[start + 1:end]

    @staticmethod
    def actions(run):
        return [(step['ref'], step['value']) for step in run if step['op'] == 'attack_with_tax']

    def test_exact_records_and_typed_scope(self):
        model.validate_witnesses(self.oracle, self.rows)
        self.assertEqual({home['csvid'] for home in self.homes.values()}, {2218, 2695, 3233, 4966, 6466, 7214, 12832, 18350})
        self.assertEqual(sum(len(home['recipes'][0]['variants']) for home in self.homes.values()), 254)
        for name, home in self.homes.items():
            recipe, = home['recipes']
            params = model.spec(recipe, self.oracle[name])
            self.assertEqual(recipe['oracle_text'], self.oracle[name]['oracle_text'])
            self.assertEqual(params['protects_walker'], name in ('Sphere of Safety', 'Baird, Steward of Argive'))
            changed = copy.deepcopy(recipe)
            changed['params']['protects_walker'] = not changed['params']['protects_walker']
            with self.assertRaises(ValueError):
                model.spec(changed, self.oracle[name])

    def test_exact_payment_or_decline_is_before_the_actual_commit(self):
        for owner in (0, 1):
            payer = 1 - owner
            for mode, paid in [('pay_exact', True), ('decline', False)]:
                run = self.first_run('Propaganda', f'{mode}_{owner}')
                declaration = self.declarations(run)
                self.assertEqual(declaration[:2], [dict(op='choose_dialog', player=payer, value=int(not paid)),
                                                   dict(op='attack_with_tax', ref='first', value=int(paid))])
                self.assertIn(dict(op='assert_card', ref='first', property='tapped', value=int(paid)), declaration)
                self.assertIn(dict(op='assert_mana', player=payer, color=0, value=0 if paid else 2), declaration)
                self.assertIn(dict(op='assert_life', player=owner, value=17 if paid else 20), run)

    def test_individual_attackers_spend_independently_and_reset_declines(self):
        for owner in (0, 1):
            for mode, attacks, life in [('two_paid',[1,1],15), ('first_only',[1,0],17), ('second_only',[0,1],18)]:
                run = self.first_run('Ghostly Prison', f'{mode}_{owner}')
                self.assertEqual(self.actions(run), list(zip(('first', 'second'), attacks)))
                choices = [row['value'] for row in self.declarations(run) if row['op'] == 'choose_dialog']
                self.assertEqual(choices, [0,0] if mode == 'two_paid' else [0] if mode == 'first_only' else [1,0])
                self.assertIn(dict(op='assert_life', player=owner, value=life), run)

    def test_stack_and_controller_scope_use_the_actual_payer(self):
        for owner in (0, 1):
            for mode, count, budget in [('stack_sources',2,4), ('opposite_source',1,2), ('own_attacker',0,0)]:
                run = self.first_run('Windborn Muse', f'{mode}_{owner}')
                payer = owner if mode == 'own_attacker' else 1 - owner
                self.assertIn(dict(op='set_mana', player=payer, color=0, value=budget), run)
                self.assertEqual([row for row in self.declarations(run) if row['op'] == 'choose_dialog'],
                                 [dict(op='choose_dialog', player=payer, value=0)] * count)
                self.assertEqual(self.actions(run), [('first',1)])

    def test_insufficient_and_ineligible_never_commit_or_spend(self):
        for owner in (0, 1):
            for mode, pool in [('underfunded',1), ('empty_pool',0), ('tapped_attacker',2), ('sick_attacker',2)]:
                run = self.first_run('Propaganda', f'{mode}_{owner}')
                self.assertEqual(self.actions(run), [('first',0)])
                dec = self.declarations(run)
                self.assertFalse(any(row['op'] == 'choose_dialog' for row in dec))
                self.assertIn(dict(op='assert_mana', player=1-owner, color=0, value=pool), dec)

    def test_sphere_counts_live_owned_enchantments_and_ignores_artifact(self):
        for owner in (0, 1):
            budgets = [('own_enchantments',3), ('opposing_enchantments',1), ('nonenchantment',1),
                       ('remove_enchantment',2), ('cast_enchantment',2), ('mixed_stack',4)]
            for mode, expected in budgets:
                run = self.first_run('Sphere of Safety', f'{mode}_{owner}')
                first = run.index(dict(op='begin_attackers'))
                endowment = [s['value'] for s in run[:first] if s['op'] == 'set_mana' and s['player'] == 1-owner and s['color'] == 0]
                self.assertEqual(endowment[-1], expected)
                self.assertEqual(self.actions(run), [('first',1)])
                self.assertIn(dict(op='assert_mana', player=1-owner, color=0, value=0), self.declarations(run))
                if mode in ('cast_enchantment', 'remove_enchantment'):
                    self.assertIn(dict(op='cast', ref='enchantment' if mode == 'cast_enchantment' else 'removal', value=1), run[:first])

    def test_real_destination_separates_walker_loyalty_and_player_life(self):
        for owner in (0, 1):
            for name, accepts in [('Propaganda',1), ('Sphere of Safety',0)]:
                run = self.first_run(name, f'walker_short_{owner}')
                self.assertEqual([row for row in self.declarations(run) if row['op'] == 'choose_dialog'],
                                 [dict(op='choose_dialog', player=1-owner, value=1)])
                self.assertIn(dict(op='cast', ref='walker', value=1), run)
                self.assertEqual(self.actions(run), [('first',accepts)])
                loyalty = [step['value'] for step in run if step['op'] == 'assert_counter']
                self.assertEqual((loyalty[0],loyalty[-1]), (3,1 if accepts else 3))
                self.assertTrue(all(row['value'] == 20 for row in run if row['op'] == 'assert_life'))

    def test_windborn_flying_respects_ground_reach_and_exact_combat(self):
        for owner in (0, 1):
            for mode, reach in [('flying_unblocked',False), ('flying_reach',True)]:
                run = self.first_run('Windborn Muse', f'{mode}_{owner}')
                self.assertEqual(self.actions(run), [('source',1)])
                self.assertIn(dict(op='block', ref='ground', other='source', value=0), run)
                self.assertEqual(dict(op='block', ref='reach', other='source', value=1) in run, reach)
                self.assertIn(dict(op='assert_life', player=1-owner, value=20 if reach else 18), run)


    def test_collective_restraint_uses_owned_basic_types_and_real_land_changes(self):
        name = 'Collective Restraint'
        for seat in (0, 1):
            for mode, budget in [('domain_zero',0), ('domain_one',1), ('domain_three',3), ('domain_five',5),
                                 ('domain_duplicate',2), ('domain_enemy',0), ('domain_dual',2),
                                 ('domain_play',2), ('domain_remove',2)]:
                run = self.first_run(name, f'{mode}_{seat}')
                before_attack = run[:run.index(dict(op='begin_attackers'))]
                mana = [s['value'] for s in before_attack if s['op'] == 'set_mana' and s['player'] == 1-seat and s['color'] == 0]
                self.assertEqual(mana[-1], budget)
                self.assertIn(dict(op='choose_dialog', player=1-seat, value=0), self.declarations(run))
                self.assertIn(dict(op='assert_mana', player=1-seat, color=0, value=0), self.declarations(run))
                self.assertEqual(self.actions(run), [('first',1)])
                if mode == 'domain_play':
                    self.assertIn(dict(op='play_land', ref='new_dual', value=1), before_attack)
                elif mode == 'domain_remove':
                    self.assertIn(dict(op='cast', ref='land_removal', value=1), before_attack)

    def test_baird_vigilance_survives_real_own_attack_and_next_block(self):
        name = 'Baird, Steward of Argive'
        for seat in (0, 1):
            run = self.first_run(name, f'vigilance_blocks_next_{seat}')
            self.assertEqual(self.actions(run), [('source',1), ('enemy_attacker',1)])
            self.assertIn(dict(op='assert_card', ref='source', property='tapped', value=0), self.declarations(run))
            first = run.index(dict(op='attack_with_tax', ref='source', value=1))
            advance = run.index(dict(op='advance_turn'))
            block = run.index(dict(op='block', ref='source', other='enemy_attacker', value=1))
            self.assertTrue(first < advance < block)
            self.assertIn(dict(op='assert_life', player=1-seat, value=18), run)
            self.assertIn(dict(op='assert_life', player=seat, value=20), run[block:])

    def test_baird_exact_fee_and_distinct_stack(self):
        name = 'Baird, Steward of Argive'
        for seat in (0, 1):
            for mode, choices in [('pay_exact',1), ('underfunded',0), ('stack_distinct',2)]:
                run = self.first_run(name, f'{mode}_{seat}')
                dec = self.declarations(run)
                self.assertEqual(sum(row['op'] == 'choose_dialog' for row in dec), choices)
                self.assertEqual(self.actions(run), [('first',int(mode != 'underfunded'))])


    def test_elephant_grass_age_payment_uses_actual_land_at_each_upkeep(self):
        for seat in (0, 1):
            run = self.first_run('Elephant Grass', f'age_paid_twice_{seat}')
            upkeeps = [idx for idx, row in enumerate(run) if row['op'] == 'upkeep_step']
            self.assertEqual(len(upkeeps), 2)
            land = run.index(dict(op='play_land', ref='second_age_land', value=1))
            self.assertTrue(upkeeps[0] < land < upkeeps[1])
            for age, end in enumerate(upkeeps, 1):
                start = next(i for i in range(end - 1, -1, -1) if run[i]['op'] == 'untap_step')
                self.assertIn(dict(op='choose_dialog', player=seat, value=0), run[start:end])
                choices = [row for row in run[start:end] if row['op'] == 'choose_mana_source']
                self.assertEqual(len(choices), age if seat == 0 else 0)
                self.assertIn(dict(op='assert_counter', ref='source', counter_kind='age', value=age), run[end:])
            lost = self.first_run('Elephant Grass', f'age_decline_{seat}')
            self.assertEqual(self.actions(lost), [('first',1)])
            self.assertFalse(any(row['op'] == 'choose_dialog' for row in self.declarations(lost)))

    def test_elephant_grass_black_and_multicolor_black_prohibition(self):
        for seat in (0, 1):
            for mode in ('black', 'black_multicolor'):
                run = self.first_run('Elephant Grass', f'{mode}_{seat}')
                self.assertEqual(self.actions(run), [('first',0)])
                decl = self.declarations(run)
                self.assertIn(dict(op='assert_unused_choices', value=1), decl)
                self.assertIn(dict(op='assert_mana', player=1-seat, color=0, value=2), decl)
                self.assertTrue(any(row['op'] == 'assert_matching_permanents' and row['value'] & 2 for row in run))

    def test_koskun_falls_taps_actual_own_creature_or_leaves(self):
        name = 'Koskun Falls'
        for seat in (0, 1):
            paid = self.first_run(name, f'upkeep_tap_{seat}')
            phase = paid.index(dict(op='upkeep_step', player=seat))
            self.assertEqual(paid[phase - 1], dict(op='choose_target', player=seat, ref='upkeep_creature'))
            self.assertIn(dict(op='assert_card', ref='upkeep_creature', property='tapped', value=1), paid[phase:])
            self.assertIn(dict(op='choose_dialog', player=1-seat, value=0), self.declarations(paid))
            for mode in ('upkeep_enemy', 'upkeep_tapped', 'upkeep_empty', 'upkeep_decline', 'world_replaced'):
                run = self.first_run(name, f'{mode}_{seat}')
                self.assertIn(dict(op='assert_zone_multiplicity', player=seat, zone='graveyard', index=2218, value=1), run)
                self.assertFalse(any(row['op'] == 'choose_dialog' for row in self.declarations(run)))
                if mode == 'upkeep_decline':
                    self.assertIn(dict(op='assert_forced_target_choices', value=0), run)
                if mode == 'world_replaced':
                    self.assertIn(dict(op='cast', ref='new_world', value=1), run)


if __name__ == '__main__':
    unittest.main()
