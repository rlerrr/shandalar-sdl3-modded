import json
import unittest

from import_duel_oracle import registry
import spore_synergy_riders_recipes as model


class SporeSynergyTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        data = model.PROFILE.parent
        cls.oracle = json.loads((data / 'oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {r['name']: r for r in registry() if r['kind'] != 'internal'}
        cls.homes = {}
        for name, pin in model.profiles()['sources'].items():
            i = pin['registry']['csvid']
            home, = (data / f'cases/cards/{i // 100:03}').glob(f'{i:05}-*.json')
            cls.homes[name] = json.loads(home.read_text(encoding='utf8'))

    def steps(self, name, mode, owner=0):
        home = self.homes[name]
        declaration = home['recipes'][0]
        params = model.spec(declaration, self.oracle[name])
        variant = f'{mode}_{owner}'
        case, = model.expand(name, home['csvid'], declaration, params, [variant], self.rows, self.oracle)
        return case['steps']

    def count(self, name, player, zone, n):
        return dict(op='assert_zone_multiplicity', player=player, zone=zone, index=self.rows[name]['csvid'], value=n)

    def test_whole_pinned_oracle_and_recipe_variants(self):
        model.validate_witnesses(self.oracle, self.rows)
        self.assertEqual({h['csvid'] for h in self.homes.values()}, {1780, 8675, 8699, 18515, 1828, 8819, 1833, 9045})
        for name, home in self.homes.items():
            record = self.oracle[name]
            self.assertEqual(home['recipes'][0]['variants'], model.variants(model.classify(record)))
            self.assertIsNotNone(model.spec(home['recipes'][0], record))
            with self.assertRaises(ValueError):
                model.classify(dict(record, oracle_text=record['oracle_text'] + '\nDraw a card.'))

    def test_real_placement_unlocks_own_or_enemy_spend_after_two_upkeeps(self):
        for mode, controller in (('own_fungus', 0), ('enemy_fungus', 1)):
            steps = self.steps('Fungal Bloom', mode)
            choose = steps.index(dict(op='choose_target', player=0, ref='maker'))
            self.assertEqual(sum(r['op'] == 'upkeep_step' and r['player'] == controller for r in steps[:choose]), 2)
            self.assertIn(dict(op='set_mana', player=0, color=3, value=2), steps[:choose])
            self.assertEqual(steps[choose + 1], dict(op='activate', ref='source', value=1))
            self.assertEqual(steps[choose + 2], dict(op='assert_unused_choices', value=0))
            pay = steps.index(dict(op='activate', ref='maker', value=1))
            self.assertIn(dict(op='can_activate', ref='maker', value=0), steps[choose:pay])
            self.assertIn(self.count(model.TOKEN, controller, 'battlefield', 1), steps[pay:])

    def test_sporesower_team_uses_actual_thelon_count_effect(self):
        steps = self.steps('Sporesower Thallid', 'team_threshold', 1)
        self.assertEqual([r['player'] for r in steps if r['op'] == 'upkeep_step'], [0, 1] * 3)
        pay = steps.index(dict(op='activate', ref='source', value=1))
        self.assertIn(dict(op='assert_card', ref='source', property='power', value=7), steps[:pay])
        self.assertIn(dict(op='assert_card', ref='source', property='power', value=4), steps[pay:])
        self.assertIn(dict(op='assert_card', ref='friend', property='power', value=5), steps[pay:])
        self.assertTrue(all(r['value'] == 2 for r in steps if r.get('ref') in ('enemy', 'ordinary') and r.get('property') == 'power'))

    def test_exile_cost_precedes_both_teams_counters(self):
        for mode, grave in (('own_grave', 1), ('enemy_grave', 0)):
            steps = self.steps(model.JUDGE, mode, 1)
            chosen = steps.index(dict(op='choose_zone_card', player=1, value=self.rows[model.MAKER]['csvid']))
            settle = steps.index(dict(op='resolve'), chosen)
            self.assertIn(self.count(model.MAKER, grave, 'exile', 1), steps[chosen:settle])
            for ref in ('friend', 'enemy'):
                self.assertIn(dict(op='assert_card', ref=ref, property='power', value=2), steps[chosen:settle])
                self.assertIn(dict(op='assert_card', ref=ref, property='power', value=3), steps[settle:])
            self.assertIn(dict(op='assert_card', ref='ordinary', property='power', value=2), steps[settle:])

    def test_tribal_fixture_clears_both_seeded_pools_before_first_real_turn(self):
        for variant in model.CASES['tribal_lord']:
            for owner in (0, 1):
                steps = self.steps('Sporecrown Thallid', variant, owner)
                first_turn = steps.index(dict(op='advance_turn'))
                self.assertEqual({(s['player'], s['color'], s['value']) for s in steps[:first_turn]
                                  if s['op'] == 'set_mana'},
                                 {(p, color, 0) for p in (0, 1) for color in range(6)})
                self.assertEqual(steps[first_turn + 1], dict(op='untap_step', player=1 - owner))

    def test_lord_own_family_stats_and_actual_removal(self):
        for mode, owner, token_pt in (('own_saproling', 0, 2), ('enemy_saproling', 1, 1), ('no_lord', 0, 1)):
            steps = self.steps('Sporecrown Thallid', mode)
            stats = [r['value'] for r in steps if r['op'] == 'assert_matching_permanents' and r.get('property') == 'power']
            self.assertEqual(stats[-1], token_pt)
            self.assertIn(dict(op='assert_card', ref='source', property='power', value=2), steps)
            self.assertIn(dict(op='assert_card', ref='friend', property='power', value=3), steps)
            self.assertIn(self.count('Sporecrown Thallid', 0, 'hand', int(mode == 'no_lord')), steps[-85:])
            self.assertTrue(all(r['player'] == owner for r in steps if r['op'] == 'assert_matching_permanents'))

    def test_saproling_paid_before_pump_with_scope_and_expiry(self):
        for name, menu, token_remaining, change in (('Thallid Devourer', 'Sac a Saproling and pump', 0, 4),
                                                       ('Pallid Mycoderm', 'Pump Fungi & Saprolings', 1, 5)):
            steps = self.steps(name, 'pump_expiry')
            choice = next(i for i, r in enumerate(steps) if r.get('mode_label') == menu)
            resolve = steps.index(dict(op='resolve'), choice)
            expiry = steps.index(dict(op='advance_turn'), resolve)
            self.assertIn(self.count(model.TOKEN, 0, 'battlefield', token_remaining), steps[choice:resolve])
            self.assertIn(dict(op='choose_target', player=0, ref='meal'), steps[choice:resolve])
            self.assertIn(dict(op='assert_card', ref='source', property='toughness', value=change), steps[resolve:expiry])
            self.assertIn(dict(op='assert_card', ref='source', property='toughness', value=int(self.oracle[name]['toughness'])),
                          steps[expiry:])
            if token_remaining:
                self.assertIn(dict(op='assert_matching_permanents', player=0, index=18813, property='power', value=2),
                              steps[resolve:expiry])

    def test_thorn_uses_all_three_actual_target_types_and_bounce_response(self):
        for mode, choice in (('player', dict(op='choose_player', player=0, value=1)),
                             ('creature', dict(op='choose_target', player=0, ref='target')),
                             ('planeswalker', dict(op='choose_target', player=0, ref='walker'))):
            steps = self.steps('Thorn Thallid', mode)
            pay = steps.index(dict(op='activate', ref='source', value=1))
            self.assertIn(choice, steps[:pay])
            self.assertIn(dict(op='can_activate', ref='source', value=0), steps[pay:])
        steps = self.steps('Thorn Thallid', 'missing_target')
        cast = steps.index(dict(op='cast', ref='bounce', value=1))
        self.assertEqual(steps[cast + 1:cast + 3], [dict(op='assert_stack', value=2), dict(op='resolve')])
        self.assertIn(self.count(model.GOBLIN, 1, 'hand', 1), steps)

    def test_all_five_colors_are_real_nonstack_sacrifice_mana(self):
        for mode, color in model.COLORS.items():
            for owner in (0, 1):
                steps = self.steps('Utopia Mycon', mode, owner)
                choose = steps.index(dict(op='choose_color', player=owner, value=color))
                self.assertIn(dict(op='activate', ref='source', value=1), steps[choose:])
                self.assertIn(dict(op='require_activation_mode', value=1), steps[choose:])
                self.assertIn(dict(op='assert_mana', player=owner, color=color, value=1), steps[choose:])
                self.assertNotIn(dict(op='resolve'), steps[choose:])
                self.assertIn(dict(op='assert_stack', value=0), steps[choose:])
                self.assertIn(self.count(model.TOKEN, owner, 'battlefield', 0), steps[choose:])
