"""Check named-exile plans, exact owners, and real cast histories."""

import json
from pathlib import Path
import unittest

from generate_cases import registry
import spell_named_exile_recipes as model


DUEL = Path(__file__).resolve().parent


class SpellNamedExileRecipes(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle = json.loads((DUEL / 'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {row['name']: row for row in registry()}
        cls.homes = {}
        for name in model.profile()['sources']:
            csvid = cls.rows[name]['csvid']
            path, = (DUEL / f'data/cases/cards/{csvid // 100:03d}').glob(f'{csvid:05d}-*.json')
            cls.homes[name] = json.loads(path.read_text(encoding='utf8'))

    def case(self, name, variant):
        home = self.homes[name]
        declaration, = home['recipes']
        params = model.spec(declaration, self.oracle[name])
        case, = model.expand(name, home['csvid'], declaration, params, [variant], self.rows, self.oracle)
        return case

    def copies(self, name, actor, zone, value):
        return dict(op='assert_zone_multiplicity', player=actor, zone=zone, index=self.rows[name]['csvid'], value=value)

    def test_catalog_full_oracle_exact_cost_and_real_handlers(self):
        model.validate_witnesses(self.oracle, self.rows)
        self.assertEqual({home['csvid'] for home in self.homes.values()}, {4121, 4188, 4193, 4194, 15302})
        self.assertEqual(sum(len(home['recipes'][0]['variants']) for home in self.homes.values()), 184)
        for name, home in self.homes.items():
            declaration, = home['recipes']
            record = self.oracle[name]
            self.assertEqual(declaration['variants'], model.variants(model.spec(declaration, record)))
            with self.assertRaises(ValueError):
                model.classify(dict(record, oracle_text=record['oracle_text'] + '\nDraw a card.'))

    def test_whole_effect_removes_two_copies_per_real_zone(self):
        for name in self.homes:
            target = model.LEGAL[model.classify(self.oracle[name])['scope']]
            for actor in (0, 1):
                steps = self.case(name, f'whole_{actor}')['steps']
                cast = steps.index(dict(op='cast', ref='source', value=1))
                resolve = steps.index(dict(op='resolve'))
                selected = [s for s in steps[cast:resolve] if s['op'] == 'choose_zone_card']
                self.assertEqual(selected, [dict(op='choose_zone_card', player=actor, value=self.rows[target]['csvid'])] * 6)
                self.assertIn(self.copies(target, 1 - actor, 'exile', 7), steps[resolve:])
                self.assertIn(self.copies(target, actor, 'exile', 0), steps[resolve:])
                for zone in model.ZONES:
                    self.assertIn(self.copies(target, 1 - actor, zone, 0), steps[resolve:])
                    self.assertIn(self.copies(target, actor, zone, 1), steps[resolve:])

    def test_controller_and_physical_owner_are_distinct_roles(self):
        name = 'Crumble to Dust'
        target = model.LEGAL['land']
        for actor in (0, 1):
            cross = self.case(name, f'cross_owned_{actor}')
            anchor, = [card for card in cross['setup'] if card['ref'] == 'target']
            self.assertEqual(anchor['player'], 1 - actor)
            self.assertIn('opponent_owned', anchor['state'])
            after = cross['steps'][cross['steps'].index(dict(op='resolve')):]
            self.assertIn(self.copies(target, actor, 'exile', 1), after)
            self.assertIn(self.copies(target, 1 - actor, 'exile', 6), after)
            self.assertIn(dict(op='assert_shuffle_count', player=1 - actor, value=1), after)
            own = self.case(name, f'own_controller_{actor}')['steps']
            after = own[own.index(dict(op='resolve')):]
            self.assertIn(self.copies(target, actor, 'exile', 7), after)
            self.assertIn(self.copies(target, 1 - actor, 'exile', 0), after)
            self.assertIn(dict(op='assert_shuffle_count', player=actor, value=1), after)

    def test_crumble_can_select_some_or_none_in_each_zone(self):
        name = 'Crumble to Dust'
        target = model.LEGAL['land']
        for actor in (0, 1):
            for mode, take in [('partial', 1), ('choose_none', 0)]:
                steps = self.case(name, f'{mode}_{actor}')['steps']
                resolve = steps.index(dict(op='resolve'))
                self.assertEqual(sum(s['op'] == 'choose_zone_card' for s in steps[:resolve]), 3 * take)
                self.assertEqual(sum(s['op'] == 'choose_optional_zone' for s in steps[:resolve]), 3)
                self.assertIn(dict(op='assert_forced_zone_choices', value=0), steps[resolve:])
                self.assertIn(self.copies(target, 1 - actor, 'exile', 1 + 3 * take), steps[resolve:])
                for zone in model.ZONES:
                    self.assertIn(self.copies(target, 1 - actor, zone, 2 - take), steps[resolve:])

    def test_empty_and_found_none_still_shuffle_and_no_ambiguous_order(self):
        for name in self.homes:
            for actor in (0, 1):
                for mode in ('no_matches', 'empty_search'):
                    steps = self.case(name, f'{mode}_{actor}')['steps']
                    resolve = steps.index(dict(op='resolve'))
                    self.assertFalse(any(s['op'] == 'choose_zone_card' for s in steps[:resolve]))
                    after = steps[resolve:]
                    self.assertIn(dict(op='assert_shuffle_count', player=1 - actor, value=1), after)
                    self.assertIn(dict(op='assert_shuffle_count', player=actor, value=0), after)
                    self.assertFalse(any(s['op'] == 'assert_zone_card' and s['player'] == 1 - actor and s['zone'] == 'library' for s in after))

    def test_real_cast_and_target_loss_use_actual_priority(self):
        for name in self.homes:
            for actor in (0, 1):
                run = self.case(name, f'target_leaves_{actor}')['steps']
                announce = run.index(dict(op='announce_spell', ref='source'))
                bounce = run.index(dict(op='cast_response', ref='reply', value=1))
                self.assertLess(announce, bounce)
                self.assertIn(dict(op='choose_target', player=1 - actor, ref='target'), run[announce:bounce])
                self.assertFalse(any(s['op'].startswith('choose_zone') for s in run))
                self.assertIn(dict(op='assert_shuffle_count', player=1 - actor, value=0), run[bounce:])
                wrong = self.case(name, f'wrong_target_{actor}')['steps']
                self.assertIn(dict(op='cast', ref='source', value=0), wrong)

    def test_devoid_counter_is_a_real_spell_gate_with_red_payment(self):
        for actor in (0, 1):
            for name, allowed in [('Sowing Salt', 0), ('Crumble to Dust', 1)]:
                steps = self.case(name, f'color_gate_{actor}')['steps']
                cast = steps.index(dict(op='cast', ref='source', value=1))
                self.assertIn(dict(op='set_mana', player=actor, color=4, value=1 if allowed else 2), steps[:cast])
                reply = steps.index(dict(op='cast_response', ref='reply', value=allowed))
                self.assertIn(dict(op='set_mana', player=1 - actor, color=2, value=1), steps[cast:reply])
                self.assertIn(dict(op='announce_spell', ref='source'), steps[cast:reply])
                self.assertIn(dict(op='assert_shuffle_count', player=1 - actor, value=1 - allowed), steps[reply:])

    def test_search_plan_replays_exactly_and_does_not_touch_other_matching_permanent(self):
        for name in self.homes:
            steps = self.case(name, 'replay_1')['steps']
            cut = steps.index(dict(op='capture'))
            record = steps.index(dict(op='record_state'))
            restore = steps.index(dict(op='restore'))
            self.assertEqual(steps[cut + 1:record], steps[restore + 1:-1])
            self.assertEqual(steps[-1], dict(op='assert_recorded_state'))
            run = self.case(name, 'battlefield_copy_1')['steps']
            resolve = run.index(dict(op='resolve'))
            self.assertIn(dict(op='assert_card', ref='other_same', property='in_play', value=1), run[resolve:])
