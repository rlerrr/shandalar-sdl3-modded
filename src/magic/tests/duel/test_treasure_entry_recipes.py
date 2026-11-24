"""Focused Treasure-entry grammar, fixture and production-action plan checks."""

import copy
import json
from pathlib import Path
import unittest

from generate_cases import registry
from recipes import COLORS
import treasure_entry_recipes as model


DUEL = Path(__file__).resolve().parent


class TreasureEntryRecipes(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.oracle = json.loads((DUEL / 'data/oracle.json').read_text(encoding='utf8'))['cards']
        cls.rows = {row['name']: row for row in registry()}
        cls.homes = {}
        for name in model.pins()['sources']:
            csvid = cls.rows[name]['csvid']
            home, = (DUEL / f'data/cases/cards/{csvid // 100:03d}').glob(f'{csvid:05d}-*.json')
            cls.homes[name] = json.loads(home.read_text(encoding='utf8'))

    def case(self, name, variant):
        home = self.homes[name]
        recipe, = home['recipes']
        params = model.spec(recipe, self.oracle[name])
        case, = model.expand(name, home['csvid'], recipe, params, [variant], self.rows, self.oracle)
        return case

    @staticmethod
    def first(case):
        return case['steps'][:case['steps'].index(dict(op='record_state'))]

    def zone(self, run, name, seat, zone='battlefield'):
        csvid = self.rows[name]['csvid']
        return [step['value'] for step in run if step['op'] == 'assert_zone_multiplicity' and
                (step['index'], step['player'], step['zone']) == (csvid, seat, zone)][-1]

    def test_whole_oracle_typed_pins_and_counts(self):
        model.validate_witnesses(self.oracle, self.rows)
        self.assertEqual({home['csvid'] for home in self.homes.values()}, {17484, 17512, 17591, 18168, 18169, 18262})
        self.assertEqual(sum(len(home['recipes'][0]['variants']) for home in self.homes.values()), 208)
        for name, home in self.homes.items():
            recipe, = home['recipes']
            record = self.oracle[name]
            declared = model.spec(recipe, record)
            if 'composition' not in declared:
                self.assertEqual(declared['count'], 2 if name == 'Prosperous Pirates' else 1)
            self.assertEqual(set(recipe['variants']), set(model.variants(declared)))
            altered = copy.deepcopy(recipe)
            altered['params']['count'] = True
            with self.assertRaises(ValueError):
                model.spec(altered, record)
            with self.assertRaises(ValueError):
                model.classify(dict(record, oracle_text=record['oracle_text'] + '\nHaste'))

    def test_real_entry_generated_token_identity_and_replay(self):
        for owner in (0, 1):
            for name, count in [('Prosperous Pirates', 2), ('Wily Goblin', 1)]:
                case = self.case(name, f'entry_{owner}')
                run = self.first(case)
                self.assertNotIn('Treasure', [card['name'] for card in case['setup']])
                self.assertEqual(run[2:15], [dict(op='set_mana', player=seat, color=color, value=0)
                    for seat in (0, 1) for color in range(6)] + [dict(op='refresh')])
                resolve = run.index(dict(op='resolve'))
                self.assertEqual(self.zone(run[:resolve], 'Treasure', owner), 0)
                self.assertEqual(self.zone(run, 'Treasure', owner), count)
                self.assertEqual(self.zone(run, 'Treasure', 1-owner), 0)
                for prop, value in [('token', 1), ('artifact', 1), ('color', 0), ('power', 0),
                                    ('toughness', 0), ('tapped', 0)]:
                    self.assertIn(dict(op='assert_matching_permanents', player=owner, index=18840,
                                       property=prop, value=value), run[resolve:])
                self.assertEqual(case['steps'][len(run)+2:], run[1:] + [dict(op='assert_recorded_state')])

    def test_pending_failed_and_countered_entry_require_actual_resolution(self):
        name = 'Brazen Freebooter'
        for owner in (0, 1):
            for mode in ('wrong_color', 'short_cost', 'board_no_entry', 'unrelated_entry', 'countered'):
                run = self.first(self.case(name, f'{mode}_{owner}'))
                if mode == 'board_no_entry':
                    turn = run.index(dict(op='advance_turn'))
                    count = model.classify(self.oracle[name])['count']
                    self.assertEqual(self.zone(run[:turn], 'Treasure', owner), count)
                    self.assertEqual(self.zone(run[turn:], 'Treasure', owner), count)
                    self.assertEqual(self.zone(run, 'Treasure', 1-owner), 0)
                    self.assertFalse(any(step['op'] in ('cast', 'resolve') for step in run))
                else:
                    self.assertEqual(self.zone(run, 'Treasure', owner), 0)
                if mode == 'countered':
                    self.assertIn(dict(op='announce_spell', ref='source'), run)
                    self.assertIn(dict(op='cast_response', ref='counter', value=1), run)
                    self.assertIn(dict(op='resolve_response'), run)
                    self.assertEqual(self.zone(run, name, owner, 'graveyard'), 1)
                elif mode in ('wrong_color', 'short_cost'):
                    self.assertIn(dict(op='cast', ref='source', value=0), run)
                    self.assertEqual(self.zone(run, name, owner, 'hand'), 1)

    def test_second_entry_blink_bounce_and_both_controllers(self):
        name = 'Sailor of Means'
        for mode in ('two_entries', 'bounce_recast', 'blink'):
            run = self.first(self.case(name, f'{mode}_1'))
            self.assertEqual(self.zone(run, 'Treasure', 1), 2)
            if mode == 'blink':
                self.assertIn(dict(op='choose_target', ref='source', player=1), run)
                self.assertIn(dict(op='bind_permanent_copy', ref='source', player=1, value=17512, index=0), run)
            if mode == 'bounce_recast':
                self.assertEqual(run.count(dict(op='cast', ref='source', value=1)), 2)
        run = self.first(self.case(name, 'both_players_0'))
        self.assertIn(dict(op='advance_turn'), run)
        self.assertEqual([self.zone(run, 'Treasure', owner) for owner in (0,1)], [1,1])

    def test_all_five_real_mana_colors_sacrifice_and_leave_no_token_card(self):
        for owner in (0, 1):
            for mode, color in model.MANA_MODES.items():
                run = self.first(self.case('Wily Goblin', f'{mode}_{owner}'))
                activate = run.index(dict(op='activate', ref='treasure', value=1))
                self.assertEqual(run[activate-1], dict(op='choose_color', player=owner, value=COLORS[color]))
                self.assertIn(dict(op='assert_mana', player=owner, color=COLORS[color], value=1), run[activate:])
                self.assertIn(dict(op='activate', ref='treasure', value=0), run[activate:])
                self.assertEqual(self.zone(run, 'Treasure', owner), 0)
                self.assertEqual(self.zone(run, 'Treasure', owner, 'graveyard'), 0)
                self.assertEqual(self.zone(run, 'Treasure', owner, 'exile'), 0)

    def test_real_tap_prevents_use_until_an_actual_untap(self):
        run = self.first(self.case('Wily Goblin', 'tap_untap_1'))
        denied = run.index(dict(op='activate', ref='treasure', value=0))
        allowed = run.index(dict(op='activate', ref='treasure', value=1))
        self.assertIn(dict(op='cast', ref='tap_spell', value=1), run[:denied])
        self.assertIn(dict(op='assert_card', ref='treasure', property='tapped', value=1), run[:denied])
        self.assertIn(dict(op='cast', ref='untap_spell', value=1), run[denied:allowed])
        self.assertIn(dict(op='assert_card', ref='treasure', property='tapped', value=0), run[denied:allowed])

    def test_real_one_and_two_token_spending_without_refunding(self):
        for name, mode, colors in [('Sailor of Means', 'spend_blue', ['U']),
                                   ('Prosperous Pirates', 'pay_red_pair', ['R', 'R'])]:
            run = self.first(self.case(name, f'{mode}_1'))
            activations = [i for i, s in enumerate(run) if s == dict(op='activate', ref='treasure', value=1)]
            self.assertEqual([run[i-1]['value'] for i in activations], [COLORS[c] for c in colors])
            cast = run.index(dict(op='cast', ref='spending', value=1))
            self.assertNotIn('set_mana', [step['op'] for step in run[activations[0]:cast]])
            self.assertIn(dict(op='assert_mana', player=1, color=COLORS[colors[0]], value=0), run[cast:])
            if mode == 'spend_blue':
                self.assertEqual(self.zone(run, 'Reach Through Mists', 1, 'graveyard'), 1)
                self.assertIn(dict(op='assert_zone', player=1, zone='library', value=1), run[cast:])
            else:
                self.assertEqual(self.zone(run, 'Wily Goblin', 1), 1)
                self.assertEqual(self.zone(run, 'Treasure', 1), 1)

    def test_removed_source_and_wrong_controller_do_not_share_or_remove_treasure(self):
        name = 'Brazen Freebooter'
        run = self.first(self.case(name, 'source_removed_0'))
        cast = run.index(dict(op='cast', ref='change', value=1))
        self.assertIn(dict(op='activate', ref='treasure', value=1), run[cast:])
        self.assertEqual(self.zone(run, name, 0, 'graveyard'), 1)
        wrong = self.first(self.case(name, 'wrong_player_cannot_spend_1'))
        self.assertIn(dict(op='cast', ref='spending', value=0), wrong)
        self.assertEqual(self.zone(wrong, 'Treasure', 1), 1)
        self.assertEqual(self.zone(wrong, 'Reach Through Mists', 0, 'hand'), 1)

    def test_whole_compositions_preserve_printed_scope(self):
        for name, category in [("Brass's Bounty", 'lands'), ('Pitiless Plunderer', 'death')]:
            record = self.oracle[name]
            recipe, = self.homes[name]['recipes']
            self.assertEqual(model.spec(recipe, record)['composition'], category)
            text = record['oracle_text'].replace('you control', 'an opponent controls')
            with self.assertRaises(ValueError):
                model.classify(dict(record, oracle_text=text))

    def test_bounty_counts_real_current_lands_by_permanent(self):
        name = "Brass's Bounty"
        for owner in (0, 1):
            for mode, amount in [('zero', 0), ('opposing_land', 0), ('nonland', 0),
                                 ('dual_land', 1), ('tapped_land', 1), ('three', 3), ('played_land', 1)]:
                run = self.first(self.case(name, f'{mode}_{owner}'))
                self.assertEqual(self.zone(run, 'Treasure', owner), amount)
                self.assertEqual(self.zone(run, 'Treasure', 1-owner), 0)
                self.assertIn(dict(op='cast', ref='source', value=1), run)
                self.assertEqual(self.zone(run, name, owner, 'graveyard'), 1)
                if mode == 'played_land':
                    self.assertIn(dict(op='play_land', ref='land_0', value=1), run)

    def test_bounty_rechecks_count_after_actual_response(self):
        run = self.first(self.case("Brass's Bounty", 'response_land_loss_0'))
        self.assertIn(dict(op='announce_spell', ref='source'), run)
        response = run.index(dict(op='cast_response', ref='response_spell', value=1))
        resolve = run.index(dict(op='resolve_response'))
        self.assertIn(dict(op='choose_target', player=1, ref='land_0'), run[:response])
        self.assertEqual(self.zone(run[:resolve], 'Treasure', 0), 0)
        self.assertEqual(self.zone(run, 'Tropical Island', 0, 'graveyard'), 1)
        self.assertEqual(self.zone(run, 'Treasure', 0), 1)

    def test_other_creature_death_counts_once_and_ignores_non_death(self):
        for mode, count in [('own_death', 1), ('own_dies_off_turn', 1), ('foe_death', 0),
                            ('self_death', 0), ('artifact_destruction', 0), ('own_exiled', 0),
                            ('own_bounced', 0), ('death_before_entry', 0), ('death_after_source', 0),
                            ('two_deaths', 2), ('simultaneous_source', 2)]:
            run = self.first(self.case('Pitiless Plunderer', f'{mode}_0'))
            self.assertIn(dict(op='cast', ref='kill', value=1), run)
            self.assertEqual(self.zone(run, 'Treasure', 0), count)
            self.assertEqual(self.zone(run, 'Treasure', 1), 0)
            if mode in ('self_death', 'simultaneous_source'):
                self.assertEqual(self.zone(run, 'Pitiless Plunderer', 0, 'graveyard'), 1)
            if mode == 'two_deaths':
                self.assertIn(dict(op='assert_card', ref='source', property='damage', value=2), run)

    def test_borrowed_and_generated_creatures_are_actual_death_witnesses(self):
        for owner in (0, 1):
            card = 'Pitiless Plunderer'
            run = self.first(self.case(card, f'borrowed_creature_{owner}'))
            self.assertIn(dict(op='cast', ref='control', value=1), run)
            self.assertEqual(self.zone(run, 'Grizzly Bears', 1-owner, 'graveyard'), 1)
            self.assertEqual(self.zone(run, 'Control Magic', owner, 'graveyard'), 1)
            self.assertEqual(self.zone(run, 'Treasure', owner), 1)
            run = self.first(self.case(card, f'token_destruction_{owner}'))
            self.assertIn(dict(op='cast', ref='token_spell', value=1), run)
            self.assertEqual(self.zone(run, 'Goblin', owner), 1)
            self.assertEqual(self.zone(run, 'Goblin', owner, 'graveyard'), 0)
            self.assertEqual(self.zone(run, 'Treasure', owner), 1)


if __name__ == '__main__':
    unittest.main()
