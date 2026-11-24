"""Verify whole-Oracle triggers, physical roles, and real upkeep and death plans."""

import json
from pathlib import Path
import unittest

from generate_cases import registry
import graveyard_recursion_triggers_recipes as model


DUEL = Path(__file__).resolve().parent


class GraveyardRecursionTriggersRecipes(unittest.TestCase):
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
        recipe, = home['recipes']
        params = model.spec(recipe, self.oracle[name])
        case, = model.expand(name, home['csvid'], recipe, params, [variant], self.rows, self.oracle)
        return case

    def copies(self, name, who, zone, count):
        return dict(op='assert_zone_multiplicity', player=who, zone=zone, index=self.rows[name]['csvid'], value=count)

    def test_full_oracle_cost_stats_and_catalog(self):
        model.validate_witnesses(self.oracle, self.rows)
        self.assertEqual({item['csvid'] for item in self.homes.values()}, {5091, 6219, 7024, 10535, 11873, 12355})
        self.assertEqual(sum(len(item['recipes'][0]['variants']) for item in self.homes.values()), 196)
        for name, home in self.homes.items():
            recipe, = home['recipes']
            record = self.oracle[name]
            self.assertEqual(recipe['variants'], model.variants(model.spec(recipe, record)))
            with self.assertRaises(ValueError):
                model.classify(dict(record, oracle_text=record['oracle_text'] + '\nDraw a card.'))

    def test_upkeep_target_and_controller_are_real_and_distinct_from_owner(self):
        for name, candidate, zone in [('Reya Dawnbringer', 'Grizzly Bears', 'battlefield'),
                                      ('Angel of Flight Alabaster', model.SPIRITS[0], 'hand'),
                                      ('Bringer of the White Dawn', model.ARTIFACTS[0], 'battlefield'),
                                      ('Oversold Cemetery', 'Grizzly Bears', 'hand')]:
            for who in (0, 1):
                case = self.case(name, f'cross_owned_{who}')
                source, = [card for card in case['setup'] if card['ref'] == 'source']
                self.assertIn('opponent_owned', source['state'])
                run = case['steps']
                first = run.index(dict(op='upkeep_step', player=1 - who))
                last = run.index(dict(op='upkeep_step', player=who))
                pick = run.index(dict(op='choose_zone_card', player=who, value=self.rows[candidate]['csvid']))
                self.assertLess(first, pick)
                self.assertLess(pick, last)
                self.assertIn(self.copies(candidate, who, zone, 1), run[last:])
                self.assertIn(self.copies(candidate, 1 - who, 'graveyard', 1), run[last:])

    def test_artifact_and_spirit_filters_cover_both_eligible_choices(self):
        for name, candidate, illegal in [('Bringer of the White Dawn', 'Ornithopter', 'Suntail Hawk'),
                                        ('Angel of Flight Alabaster', 'Hana Kami', 'Reach Through Mists')]:
            for who in (0, 1):
                run = self.case(name, f'alternate_{who}')['steps']
                self.assertIn(dict(op='choose_zone_card', player=who, value=self.rows[candidate]['csvid']), run)
                self.assertIn(dict(op='assert_offered_legal_card', index=self.rows[illegal]['csvid'], value=0), run)
                denial = self.case(name, f'shroud_{who}')['steps']
                self.assertFalse(any(action['op'] == 'choose_zone_card' for action in denial))
                self.assertIn(self.copies(candidate, who, 'graveyard', 1), denial[denial.index(dict(op='upkeep_step', player=who)):])

    def test_emeria_counts_basic_or_dual_plains_and_produces_white_after_untap(self):
        name = 'Emeria, the Sky Ruin'
        for who in (0, 1):
            for mode, allowed in [('subtyped_land', True), ('wrong_land', False), ('six', False),
                                  ('opponent_plains', False), ('minus_plains', False)]:
                run = self.case(name, f'{mode}_{who}')['steps']
                self.assertIn(dict(op='play_land', ref='source', value=1), run)
                self.assertEqual(any(s['op'] == 'choose_zone_card' for s in run), allowed)
                self.assertLess(run.index(dict(op='untap_step', player=who)), run.index(dict(op='upkeep_step', player=who)))
            mana = self.case(name, f'white_mana_{who}')['steps']
            land = mana.index(dict(op='play_land', ref='source', value=1))
            denied = mana.index(dict(op='activate', ref='source', value=0))
            allowed = mana.index(dict(op='activate', ref='source', value=1))
            self.assertIn(dict(op='assert_card', ref='source', property='tapped', value=1), mana[land:denied])
            self.assertLess(mana.index(dict(op='untap_step', player=who)), allowed)
            self.assertIn(dict(op='assert_mana', player=who, color=5, value=1), mana[allowed:])

    def test_cemetery_four_creatures_and_optional_decline(self):
        name = 'Oversold Cemetery'
        for who in (0, 1):
            for mode, count in [('below_threshold', 0), ('threshold_falls', 1), ('repeat', 2)]:
                run = self.case(name, f'{mode}_{who}')['steps']
                self.assertEqual(sum(step['op'] == 'choose_zone_card' for step in run), count)
            declined = self.case(name, f'decline_{who}')['steps']
            upkeep = declined.index(dict(op='upkeep_step', player=who))
            self.assertEqual(declined[upkeep - 1]['op'], 'choose_optional_zone')
            self.assertIn(dict(op='assert_forced_zone_choices', value=0), declined[upkeep:])
            self.assertIn(self.copies('Grizzly Bears', who, 'graveyard', 1), declined[upkeep:])

    def test_driver_uses_actual_death_and_inclusive_mana_value_limit(self):
        name = 'Driver of the Dead'
        for who in (0, 1):
            for mode, candidate in [('effect', 'Grizzly Bears'), ('alternate', 'Skirk Prospector'), ('zero_mv', 'Ornithopter')]:
                run = self.case(name, f'{mode}_{who}')['steps']
                threat = run.index(dict(op='cast', ref='removal', value=1))
                self.assertIn(dict(op='choose_zone_card', player=who, value=self.rows[candidate]['csvid']), run[threat:])
                self.assertIn(dict(op='assert_offered_legal_card', index=self.rows['Scathe Zombies']['csvid'], value=0), run[threat:])
                self.assertIn(self.copies(candidate, who, 'battlefield', 1), run[threat:])
            for mode in ('exile_source', 'bounce_source', 'other_dies', 'wrong_type'):
                self.assertFalse(any(action['op'] == 'choose_zone_card' for action in self.case(name, f'{mode}_{who}')['steps']))
            theft = self.case(name, f'cross_owned_{who}')['steps']
            self.assertIn(self.copies(name, 1 - who, 'graveyard', 1), theft)
            self.assertIn(self.copies('Grizzly Bears', who, 'battlefield', 1), theft)

    def test_bringers_pay_both_printed_costs_and_combat_runs_real_trample_and_flying(self):
        name = 'Bringer of the White Dawn'
        for who in (0, 1):
            for mode, menu, generic, white in [('choose_rainbow', 0, 7, 1), ('choose_normal', 1, 0, 0)]:
                run = self.case(name, f'{mode}_{who}')['steps']
                cast = run.index(dict(op='cast', ref='source', value=1))
                self.assertEqual(run[cast - 1], dict(op='choose_dialog', player=who, value=menu))
                self.assertIn(dict(op='assert_mana', player=who, color=0, value=generic), run[cast:])
                self.assertIn(dict(op='assert_mana', player=who, color=5, value=white), run[cast:])
            for creature, mode, block, life in [(name, 'trample', 1, 17),
                    ('Reya Dawnbringer', 'flying_ground', 0, 16), ('Angel of Flight Alabaster', 'flying_reach', 1, 20)]:
                run = self.case(creature, f'{mode}_{who}')['steps']
                damage = run.index(dict(op='combat_damage'))
                self.assertIn(dict(op='block', ref='blocker', other='source', value=block), run[:damage])
                self.assertIn(dict(op='assert_life', player=1 - who, value=life), run[damage:])

    def test_snapshot_replays_actual_trigger_and_receipts(self):
        for name in self.homes:
            run = self.case(name, 'replay_1')['steps']
            cut = run.index(dict(op='capture'))
            record = run.index(dict(op='record_state'))
            restore = run.index(dict(op='restore'))
            self.assertEqual(run[cut + 1:record], run[restore + 1:-1])
            self.assertEqual(run[-1], dict(op='assert_recorded_state'))
