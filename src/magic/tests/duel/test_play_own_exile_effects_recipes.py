"""Focused generator checks for real top-exile grants and composed clauses."""

import json
from pathlib import Path
import unittest

from generate_cases import registry
import play_own_exile_effects_recipes as model


DUEL = Path(__file__).resolve().parent


class PlayOwnExileEffectsRecipes(unittest.TestCase):
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
        recipe, = home['recipes']
        params = model.spec(recipe, self.oracle[name])
        case, = model.expand(name, home['csvid'], recipe, params, [variant], self.rows, self.oracle)
        return case['steps']

    def permission(self, steps, op, who, card, expected):
        probe = dict(op=op, player=who, index=self.rows[card]['csvid'], value=expected)
        return steps.index(probe)

    def test_pinned_complete_clauses_and_catalog(self):
        model.validate_witnesses(self.oracle, self.rows)
        self.assertEqual({home['csvid'] for home in self.homes.values()}, {14063, 14600, 14870, 15069, 19672})
        self.assertEqual(sum(len(home['recipes'][0]['variants']) for home in self.homes.values()), 194)
        for name, home in self.homes.items():
            recipe, = home['recipes']
            record = self.oracle[name]
            params = model.spec(recipe, record)
            self.assertCountEqual(recipe['variants'], model.variants(params))
            with self.assertRaises(ValueError):
                model.spec(dict(recipe, params=dict(recipe['params'], next_turn=1)), record)
            with self.assertRaises(ValueError):
                model.classify(dict(record, oracle_text=record['oracle_text'] + '\nDraw a card.'))

    def test_real_land_allowance_and_unique_wrong_player_controls(self):
        for name in self.homes:
            for who in (0, 1):
                run = self.actions(name, f'spent_land_drop_{who}')
                hand_land = run.index(dict(op='play_land', ref='land_drop', value=1))
                self.assertLess(hand_land, self.permission(run, 'play_exiled', who, 'Forest', 0))
                run = self.actions(name, f'wrong_seat_{who}')
                self.assertLess(self.permission(run, 'play_exiled', 1 - who, 'Forest', 0),
                                self.permission(run, 'play_exiled', who, 'Forest', 1))
        run = self.actions('Act on Impulse', 'second_land_0')
        self.assertLess(self.permission(run, 'play_exiled', 0, 'Forest', 1),
                        self.permission(run, 'play_exiled', 0, 'Island', 0))

    def test_x_is_selected_and_paid_including_zero(self):
        for who in (0, 1):
            for mode, x, count in [('zero_x', 0, 0), ('one_x', 1, 1), ('shallow', 3, 1)]:
                run = self.actions('Commune with Lava', f'{mode}_{who}')
                cast = run.index(dict(op='cast', ref='source', value=1))
                self.assertIn(dict(op='choose_x', player=who, value=x), run[:cast])
                self.assertIn(dict(op='set_mana', player=who, color=0, value=x), run[:cast])
                self.assertIn(dict(op='set_mana', player=who, color=4, value=2), run[:cast])
                self.assertIn(dict(op='assert_mana', player=who, color=0, value=0), run[cast:])
                self.assertIn(dict(op='assert_zone', player=who, zone='exile', value=count), run[cast:])

    def test_duration_uses_real_turn_transitions_and_actions(self):
        for name in ('Commune with Lava', 'Light Up the Stage'):
            run = self.actions(name, 'expiry_0')
            positive = dict(op='can_play_exiled', player=0, index=self.rows['Forest']['csvid'], value=1)
            checks = [i for i, step in enumerate(run) if step == positive]
            self.assertEqual(len(checks), 2)
            self.assertEqual(run[checks[0]:checks[1]].count(dict(op='advance_turn')), 2)
            self.assertEqual(run[checks[1]:].count(dict(op='advance_turn')), 2)
            self.permission(run, 'play_exiled', 0, 'Forest', 0)
            run = self.actions(name, 'offturn_spell_1')
            self.assertIn(dict(op='advance_turn'), run[:self.permission(run, 'play_exiled', 1, model.SPELL, 1)])
            run = self.actions(name, 'offturn_land_1')
            self.assertIn(dict(op='advance_turn'), run[:self.permission(run, 'play_exiled', 1, 'Forest', 0)])

    def test_paid_exile_spell_combines_with_abbot_presence_and_cleanup(self):
        for who in (0, 1):
            run = self.actions('Abbot of Keral Keep', f'play_spell_{who}')
            cut = self.permission(run, 'play_exiled', who, model.SPELL, 1)
            self.assertIn(dict(op='assert_mana', player=who, color=2, value=0), run[cut:])
            self.assertIn(dict(op='assert_zone_multiplicity', player=who, zone='graveyard',
                               index=self.rows[model.SPELL]['csvid'], value=1), run[cut:])
            end = run.index(dict(op='advance_turn'), cut)
            self.assertIn(dict(op='assert_card', ref='source', property='power', value=3), run[cut:end])
            self.assertIn(dict(op='assert_card', ref='source', property='power', value=2), run[end:])
            for mode in ('own_creature', 'enemy_cast'):
                self.assertNotIn(dict(op='assert_card', ref='source', property='power', value=3),
                                 self.actions('Abbot of Keral Keep', f'{mode}_{who}'))

    def test_spectacle_requires_opponent_loss_and_distinct_payment(self):
        name = 'Light Up the Stage'
        for who in (0, 1):
            for mode, rest in [('spectacle', 2), ('spectacle_enabled', 2), ('normal_when_eligible', 0)]:
                run = self.actions(name, f'{mode}_{who}')
                cast = run.index(dict(op='cast', ref='source', value=1))
                self.assertIn(dict(op='assert_life', player=1 - who, value=18), run[:cast])
                self.assertIn(dict(op='choose_dialog', player=who, value=int(rest == 0)), run[:cast])
                self.assertIn(dict(op='assert_mana', player=who, color=0, value=rest), run[cast:])
                display = run.index(dict(op='choose_zone', player=who, value=-1), cast)
                self.assertEqual(run[display - 1], dict(op='assert_unused_choices', value=0))
                self.assertIn(dict(op='assert_offered_card', index=1, value=self.rows['Island']['csvid']), run[cast:])
            for mode in ('own_life_loss', 'old_life_loss'):
                run = self.actions(name, f'{mode}_{who}')
                self.assertIn(dict(op='cast', ref='source', value=0), run)
                self.assertFalse(any(action['op'] == 'choose_dialog' for action in run))

    def test_siege_real_work_on_upkeep_and_three_legal_target_kinds(self):
        name = 'Outpost Siege'
        for who in (0, 1):
            run = self.actions(name, f'play_land_{who}')
            first = run.index(dict(op='upkeep_step', player=1 - who))
            last = run.index(dict(op='upkeep_step', player=who))
            self.assertIn(dict(op='assert_zone', player=who, zone='exile', value=0), run[first:last])
            self.assertLess(last, self.permission(run, 'play_exiled', who, 'Forest', 1))
            noncreature = self.actions(name, f'dragons_noncreature_{who}')
            removal = noncreature.index(dict(op='cast', ref='removal', value=1))
            index = self.rows['Spellbook']['csvid']
            self.assertIn(dict(op='assert_zone_multiplicity', player=who, zone='battlefield',
                               index=index, value=1), noncreature[:removal])
            self.assertIn(dict(op='assert_zone_multiplicity', player=who, zone='graveyard',
                               index=index, value=1), noncreature[removal:])
            self.assertNotIn(dict(op='choose_player', player=who, value=1 - who), noncreature[removal:])
            self.assertIn(dict(op='assert_life', player=1 - who, value=20), noncreature[removal:])
            for mode, choice, receipt in (
                ('dragons_death', dict(op='choose_player', player=who, value=1 - who),
                 dict(op='assert_life', player=1 - who, value=19)),
                ('dragons_creature', dict(op='choose_target', player=who, ref='recipient'),
                 dict(op='assert_card', ref='recipient', property='damage', value=1)),
                ('dragons_walker', dict(op='choose_target', player=who, ref='walker'),
                 dict(op='assert_counter', ref='walker', counter_kind='loyalty', value=2)),
            ):
                run = self.actions(name, f'{mode}_{who}')
                removal = run.index(dict(op='cast', ref='removal', value=1))
                self.assertIn(dict(op='choose_dialog', player=who, value=1), run[:removal])
                self.assertLess(removal, run.index(choice))
                attack = run.index(dict(op='attack', ref='damage_boundary', value=1), removal)
                damage = run.index(dict(op='combat_damage'), attack)
                self.assertIn(dict(op='assert_choices_consumed'), run[removal:attack])
                self.assertIn(dict(op='assert_card', ref='damage_boundary', property='power', value=0), run[damage:])
                self.assertIn(receipt, run[damage:])

    def test_countered_sources_and_live_grant_snapshots(self):
        for name in self.homes:
            run = self.actions(name, 'source_countered_1')
            self.assertLess(run.index(dict(op='announce_spell', ref='source')),
                            run.index(dict(op='cast_response', ref='counter', value=1)))
            self.assertNotIn(dict(op='assert_zone', player=1, zone='exile', value=1), run)
            run = self.actions(name, 'replay_1')
            cut = run.index(dict(op='capture'))
            record = run.index(dict(op='record_state'))
            restored = run.index(dict(op='restore'))
            self.assertEqual(run[cut + 1:record], run[restored + 1:-1])
            self.assertLess(cut, self.permission(run, 'play_exiled', 1, 'Forest', 1))
