"""Focused checks for the actual owner-exile specialization scripts."""

import json
from pathlib import Path
import unittest

from generate_cases import registry
import own_exile_specializations_recipes as model


DUEL = Path(__file__).resolve().parent


class OwnExileSpecializationsRecipes(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle = json.loads((DUEL / 'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {row['name']: row for row in registry()}
        cls.homes = {}
        for name in model.profile()['sources']:
            csvid = cls.rows[name]['csvid']
            path, = (DUEL / f'data/cases/cards/{csvid // 100:03d}').glob(f'{csvid:05d}-*.json')
            cls.homes[name] = json.loads(path.read_text(encoding='utf8'))

    def actions(self, name, variant):
        home = self.homes[name]
        declaration, = home['recipes']
        params = model.spec(declaration, self.oracle[name])
        case, = model.expand(name, home['csvid'], declaration, params, [variant], self.rows, self.oracle)
        return case['steps']

    def permission(self, op, who, card, result):
        return dict(op=op, player=who, index=self.rows[card]['csvid'], value=result)

    def copy(self, name, who, zone, count):
        return dict(op='assert_zone_multiplicity', player=who, zone=zone, index=self.rows[name]['csvid'], value=count)

    def test_catalog_uses_typed_full_oracle(self):
        model.validate_witnesses(self.oracle, self.rows)
        self.assertEqual({home['csvid'] for home in self.homes.values()}, {16527, 16987, 18880})
        self.assertEqual(sum(len(home['recipes'][0]['variants']) for home in self.homes.values()), 90)
        for name, home in self.homes.items():
            declaration, = home['recipes']
            record = self.oracle[name]
            self.assertEqual(declaration['variants'], model.variants(model.spec(declaration, record)))
            with self.assertRaises(ValueError):
                model.classify(dict(record, oracle_text=record['oracle_text'] + '\nDraw a card.'))

    def test_apex_seven_exact_displays_and_ten_of_one_color(self):
        for who in (0, 1):
            for mode, color, display in (('full_prefix', 2, 7), ('shallow', 2, 2), ('empty', 2, 0),
                                         ('black', 1, 7), ('green', 3, 7), ('white', 5, 7)):
                run = self.actions('Apex of Power', f'{mode}_{who}')
                resolve = run.index(dict(op='resolve'))
                self.assertIn(dict(op='choose_color', player=who, value=color), run[:resolve])
                self.assertEqual([item['value'] for item in run if item['op'] == 'assert_offered_count'],
                                 [display] if display else [])
                self.assertIn(dict(op='assert_mana', player=who, color=color, value=10), run[resolve:])
                for other in set(range(6)) - {color}:
                    self.assertIn(dict(op='assert_mana', player=who, color=other, value=0), run[resolve:])

    def test_apex_paid_recast_and_no_land_play(self):
        for who in (0, 1):
            run = self.actions('Apex of Power', f'from_exile_{who}')
            portal = run.index(dict(op='cast', ref='portal', value=1))
            cast = run.index(self.permission('play_exiled', who, 'Apex of Power', 1))
            self.assertIn(self.copy('Apex of Power', who, 'exile', 1), run[portal:cast])
            self.assertEqual(run[cast + 1:cast + 3], [dict(op='assert_unused_choices', value=1), dict(op='resolve')])
            self.assertIn(self.copy('Apex of Power', who, 'graveyard', 1), run[cast:])
            self.assertIn(self.permission('play_exiled', who, 'Forest', 0), run[cast:])
            pay = run.index(self.permission('play_exiled', who, model.SPELL, 1))
            self.assertIn(dict(op='assert_mana', player=who, color=2, value=0), run[pay:])
            colored = self.actions('Apex of Power', f'paid_spell_blue_{who}')
            pay = colored.index(self.permission('play_exiled', who, model.SPELL, 1))
            self.assertIn(dict(op='assert_mana', player=who, color=2, value=9), colored[pay:])

    def test_spark_damage_uses_actual_mana_value_and_forbids_play(self):
        for who in (0, 1):
            for mode, card, result in (('damage', 'Grizzly Bears', 2), ('lethal', 'Air Elemental', 5),
                                       ('zero_damage', 'Forest', 0), ('own_target', 'Grizzly Bears', 2)):
                run = self.actions('Spark of Creativity', f'{mode}_{who}')
                cast = run.index(dict(op='cast', ref='source', value=1))
                resolve = run.index(dict(op='resolve'))
                self.assertIn(dict(op='choose_target', ref='target', player=who), run[:cast])
                self.assertEqual(run[resolve - 2:resolve], 2 * [dict(op='choose_dialog', player=who, value=0)])
                if result < 3:
                    self.assertIn(dict(op='assert_card', ref='target', property='damage', value=result), run[resolve:])
                else:
                    self.assertIn(self.copy('Hill Giant', 1 - who, 'graveyard', 1), run[resolve:])
                self.assertIn(self.permission('play_exiled', who, card, 0), run[resolve:])

    def test_spark_can_decline_and_requires_a_live_target(self):
        for who in (0, 1):
            run = self.actions('Spark of Creativity', f'paid_spell_{who}')
            resolve = run.index(dict(op='resolve'))
            self.assertEqual(run[resolve - 1], dict(op='choose_dialog', player=who, value=1))
            play = run.index(self.permission('play_exiled', who, model.SPELL, 1))
            self.assertIn(dict(op='set_mana', player=who, color=2, value=1), run[resolve:play])
            self.assertIn(self.copy(model.SPELL, who, 'graveyard', 1), run[play:])
            removed = self.actions('Spark of Creativity', f'removed_target_{who}')
            self.assertLess(removed.index(dict(op='announce_spell', ref='source')),
                            removed.index(dict(op='cast_response', ref='reply', value=1)))
            self.assertNotIn(dict(op='assert_zone', zone='exile', player=who, value=1), removed)

    def test_vault_uses_real_paid_threshold_and_a_free_cast(self):
        for who in (0, 1):
            run = self.actions("Oracle's Vault", f'free_funded_{who}')
            attempts = [i for i, action in enumerate(run) if action == dict(op='activate', ref='source', value=1)]
            self.assertEqual(len(attempts), 4)
            self.assertIn(dict(op='assert_card', ref='source', property='tapped', value=0), run[:attempts[0]])
            self.assertNotIn(dict(op='assert_card', ref='source', property='tapped', value=1), run[:attempts[0]])
            self.assertEqual(run[:attempts[-1]].count(dict(op='can_activate', ref='source', value=0)), 3)
            for before, after in zip(attempts, attempts[1:]):
                window = run[before:after]
                self.assertEqual(window.count(dict(op='advance_turn')), 2)
                self.assertIn(dict(op='assert_mana', player=who, color=0, value=0), window)
            last = attempts[-1]
            self.assertEqual(run[last - 1], dict(op='choose_dialog', player=who, value=1))
            cast = run.index(self.permission('play_exiled', who, model.SPELL, 1))
            self.assertIn(dict(op='assert_mana', player=who, color=0, value=2), run[cast:])
            self.assertIn(dict(op='assert_mana', player=who, color=2, value=0), run[cast:])
            self.assertIn(self.copy(model.SPELL, who, 'graveyard', 1), run[cast:])

    def test_vault_empty_third_and_zero_fee_never_seed_counters(self):
        for who in (0, 1):
            for mode in ('empty_third', 'free_zero'):
                run = self.actions("Oracle's Vault", f'{mode}_{who}')
                attempts = [i for i, action in enumerate(run) if action == dict(op='activate', ref='source', value=1)]
                self.assertEqual(len(attempts), 4)
                self.assertFalse(any(action['op'] in ('set_counter', 'assert_counter') for action in run))
                if mode == 'empty_third':
                    self.assertIn(dict(op='assert_zone', zone='library', player=who, value=0), run[attempts[-2]:attempts[-1]])
                    self.assertIn(dict(op='assert_mana', player=who, color=0, value=2), run[attempts[-1]:])
                else:
                    self.assertIn(dict(op='assert_mana', player=who, color=0, value=0), run[attempts[-1]:])
                    self.assertIn(self.permission('play_exiled', who, model.SPELL, 1), run[attempts[-1]:])

    def test_all_models_exercise_cleanup_and_replay(self):
        for name in self.homes:
            expiry = self.actions(name, 'expiry_0')
            first = expiry.index(dict(op='cleanup'))
            self.assertEqual(expiry[first + 1], dict(op='turn', player=0, value=0))
            self.assertTrue(any(item['op'] == 'play_exiled' and item['value'] == 0 for item in expiry[first:]))
            run = self.actions(name, 'replay_1')
            cut = run.index(dict(op='capture'))
            record = run.index(dict(op='record_state'))
            restore = run.index(dict(op='restore'))
            self.assertEqual(run[cut + 1:record], run[restore + 1:-1])
            self.assertEqual(run[-1], dict(op='assert_recorded_state'))
