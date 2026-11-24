"""Focused coverage checks for real Morph and printed zone effects."""

import copy
import json
from pathlib import Path
import unittest

from generate_cases import registry
import morph_turn_up_zones_recipes as model


DUEL = Path(__file__).resolve().parent


class MorphTurnUpZonesRecipes(unittest.TestCase):
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

    def run_case(self, name, variant):
        return self.case(name, variant)['steps']

    def count(self, run, player, name, zone):
        csvid = self.rows[name]['csvid']
        return [s['value'] for s in run if s['op'] == 'assert_zone_multiplicity' and
                (s['player'], s['zone'], s['index']) == (player, zone, csvid)][-1]

    def test_whole_oracle_and_typed_costs(self):
        model.validate_witnesses(self.oracle, self.rows)
        self.assertEqual({home['csvid'] for home in self.homes.values()}, {6401, 6402, 6570, 6572, 14407})
        self.assertEqual(sum(len(home['recipes'][0]['variants']) for home in self.homes.values()), 157)
        for name, home in self.homes.items():
            recipe, = home['recipes']
            record = self.oracle[name]
            params = model.spec(recipe, record)
            self.assertEqual(set(recipe['variants']), set(model.variants(params)))
            changed = copy.deepcopy(recipe)
            changed['params']['flip_mana'][0] += 1
            with self.assertRaises(ValueError):
                model.spec(changed, record)
            with self.assertRaises(ValueError):
                model.parameters(dict(record, oracle_text=record['oracle_text'] + '\nHaste'))

    def test_paid_cast_hidden_deny_and_paid_reveal_on_both_seats(self):
        for who in (0, 1):
            name = 'Root Elemental'
            hidden = self.run_case(name, f'hidden_{who}')
            self.assertIn(dict(op='cast', ref='source', value=1), hidden)
            self.assertIn(dict(op='assert_card', ref='source', property='csvid', value=model.FACE_DOWN), hidden)
            self.assertNotIn('activate', [s['op'] for s in hidden])
            wrong = self.run_case(name, f'flip_wrong_{who}')
            self.assertIn(dict(op='activate', ref='source', value=0), wrong)
            self.assertNotIn('choose_zone_card', [s['op'] for s in wrong])
            valid = self.run_case(name, f'flip_{who}')
            self.assertIn(dict(op='activate', ref='source', value=1), valid)
            self.assertIn(dict(op='assert_card', ref='source', property='csvid', value=6572), valid)
            self.assertIn(dict(op='assert_mana', player=who, color=3, value=0), valid)

    def test_dragon_filter_selection_decline_and_shuffle(self):
        name = 'Imperial Hellkite'
        serpent = self.rows['Shivan Dragon']['csvid']
        for who in (0, 1):
            run = self.run_case(name, f'changeling_offer_{who}')
            self.assertIn(dict(op='choose_zone_card', player=who, value=serpent), run)
            self.assertIn(dict(op='assert_offered_legal_card', index=9103, value=1), run)
            self.assertIn(dict(op='assert_offered_legal_card', index=2493, value=0), run)
            self.assertEqual(self.count(run, who, 'Shivan Dragon', 'hand'), 1)
            self.assertIn(dict(op='assert_shuffle_count', player=who, value=1), run)
        declined = self.run_case(name, 'decline_search_0')
        self.assertIn(dict(op='choose_dialog', player=0, value=1), declined)
        self.assertNotIn(dict(op='assert_shuffle_count', player=0, value=1), declined)
        no_match = self.run_case(name, 'opponent_only_1')
        self.assertIn(dict(op='assert_shuffle_count', player=1, value=1), no_match)
        self.assertEqual(self.count(no_match, 0, 'Shivan Dragon', 'library'), 1)

    def test_grave_return_owners_non_targeting_and_kindred(self):
        name = 'Infernal Caretaker'
        for mode in ('graveyard_shroud', 'kindred'):
            run = self.run_case(name, f'{mode}_0')
            for who in (0, 1):
                self.assertEqual(self.count(run, who, 'Scathe Zombies', 'hand'), 1)
                self.assertEqual(self.count(run, who, 'Festering Goblin', 'hand'), 1)
                self.assertEqual(self.count(run, who, 'Goblin Tinkerer', 'graveyard'), 1)
                if mode == 'kindred':
                    self.assertEqual(self.count(run, who, 'Crib Swap', 'hand'), 1)
            self.assertNotIn('choose_target', [s['op'] for s in run])

    def test_discard_before_ordered_draw_and_partial_hands(self):
        name = 'Riptide Survivor'
        for who in (0, 1):
            for mode, amount in [('empty_hand', 0), ('one_card', 1), ('retain_one', 2)]:
                run = self.run_case(name, f'{mode}_{who}')
                self.assertEqual(len([s for s in run if s['op'] == 'choose_zone_card']), amount if who == 0 else 0)
                self.assertIn(dict(op='assert_zone_card', player=who, zone='library', index=0, value=11586), run)
                drawn = [self.count(run, who, card, 'hand') for card in ('Forest', 'Island', 'Mountain')]
                self.assertEqual(drawn, [1, 1, 1])
                if amount:
                    self.assertEqual(self.count(run, who, 'Darksteel Relic', 'graveyard'), 1 if who == 0 else amount)

    def test_real_hand_entry_and_optional_filter(self):
        for who in (0, 1):
            run = self.run_case('Root Elemental', f'entry_draw_{who}')
            self.assertEqual(self.count(run, who, 'Elvish Visionary', 'battlefield'), 1)
            self.assertEqual(self.count(run, who, 'Forest', 'hand'), 1)
            self.assertIn(dict(op='assert_offered_legal_card', index=11586, value=0), run)
            self.assertNotIn(dict(op='cast', ref='entrant', value=1), run)
            refused = self.run_case('Root Elemental', f'decline_{who}')
            self.assertIn(dict(op='assert_offered_selection_required', value=0), refused)
            self.assertEqual(self.count(refused, who, 'Shivan Dragon', 'hand'), 1)

    def test_tapped_return_source_exclusion_and_other_owner(self):
        name = 'Thousand Winds'
        for who in (0, 1):
            mirror = self.run_case(name, f'tapped_source_{who}')
            self.assertEqual(self.count(mirror, who, name, 'battlefield'), 1)
            self.assertIn(dict(op='assert_card', ref='source', property='tapped', value=1), mirror)
            self.assertEqual(self.count(mirror, who, 'Darksteel Relic', 'battlefield'), 1)
            self.assertEqual(self.count(mirror, 1-who, 'Scathe Zombies', 'hand'), 1)
            donated = self.run_case(name, f'foreign_owner_{who}')
            self.assertIn(dict(op='activate', ref='trader', value=1), donated)
            self.assertEqual(self.count(donated, who, 'Goblin Tinkerer', 'hand'), 1)

    def test_flying_combat_and_replayed_reveal_boundaries(self):
        for name in ('Imperial Hellkite', 'Thousand Winds'):
            for mode, block in [('flying_ground', False), ('flying_reach', True), ('hidden_combat', True)]:
                run = self.run_case(name, f'{mode}_0')
                self.assertIn(dict(op='block', ref='blocker', other='source', value=int(block)), run)
                self.assertIn(dict(op='combat_damage'), run)
            run = self.run_case(name, 'replay_1')
            capture = run.index(dict(op='capture'))
            record = run.index(dict(op='record_state'))
            restored = run.index(dict(op='restore'))
            self.assertEqual(run[capture + 1:record], run[restored + 1:-1])
            self.assertEqual(run[-1], dict(op='assert_recorded_state'))


if __name__ == '__main__':
    unittest.main()
