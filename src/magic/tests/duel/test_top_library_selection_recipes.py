"""Guard the complete bounded selection, partition, and printed life effects."""

import json
from pathlib import Path
import unittest

from generate_cases import registry
import top_library_selection_recipes as model


DUEL = Path(__file__).resolve().parent


class TopLibrarySelectionRecipes(unittest.TestCase):
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

    def effect(self, name, variant):
        run = self.case(name, variant)['steps']
        return run[run.index(dict(op='resolve')) + 1:]

    def copies(self, name, who, zone, count):
        return dict(op='assert_zone_multiplicity', player=who, zone=zone, index=self.rows[name]['csvid'], value=count)

    def test_catalog_whole_oracle_and_cost(self):
        model.validate_witnesses(self.oracle, self.rows)
        self.assertEqual({row['csvid'] for row in self.homes.values()}, {4255, 10503, 12739, 13293, 13411, 14228, 14372, 15133, 15917, 16845})
        self.assertEqual(sum(len(row['recipes'][0]['variants']) for row in self.homes.values()), 270)
        for name, home in self.homes.items():
            declaration, = home['recipes']
            record = self.oracle[name]
            self.assertEqual(declaration['variants'], model.variants(model.spec(declaration, record)))
            with self.assertRaises(ValueError):
                model.classify(dict(record, oracle_text=record['oracle_text'] + '\nDraw a card.'))

    def test_required_single_choice_is_bounded_and_real(self):
        for name in ('Glimpse the Future', 'Strategic Planning'):
            for who in (0, 1):
                case = self.case(name, f'effect_{who}')
                run = case['steps']
                choices = [s for s in run if s['op'] == 'choose_zone_card']
                self.assertEqual(choices, [dict(op='choose_zone_card', player=who, value=self.rows['Grizzly Bears']['csvid'])])
                result = self.effect(name, f'effect_{who}')
                self.assertIn(self.copies('Grizzly Bears', who, 'hand', 1), result)
                self.assertIn(self.copies('Forest', who, 'graveyard', 1), result)
                self.assertIn(dict(op='assert_offered_count', value=3), result)
                self.assertIn(dict(op='assert_zone_card', player=who, zone='library', index=0, value=self.rows['Shock']['csvid']), result)

    def test_bitter_keeps_life_loss_for_empty_and_real_two_pick(self):
        name = 'Bitter Revelation'
        for who in (0, 1):
            for mode, life in [('effect', 18), ('short', 18), ('empty', 18), ('low_life', 1)]:
                self.assertIn(dict(op='assert_life', player=who, value=life), self.effect(name, f'{mode}_{who}'))
            self.assertIn(dict(op='assert_offered_selection_required', value=1), self.effect(name, f'effect_{who}'))
            short = self.case(name, f'short_{who}')['steps']
            self.assertEqual(sum(s['op'] == 'choose_zone_card' for s in short), 1)
            full = self.case(name, f'effect_{who}')['steps']
            self.assertEqual(sum(s['op'] == 'choose_zone_card' for s in full), 2)
            self.assertIn(dict(op='assert_shuffle_count', player=who, value=0), self.effect(name, f'effect_{who}'))

    def test_beast_automatic_exact_creatures_and_immediate_outside_boundary(self):
        name = 'Beast Hunt'
        for who in (0, 1):
            run = self.case(name, f'effect_{who}')['steps']
            self.assertFalse(any(s['op'].startswith('choose_') for s in run))
            result = self.effect(name, f'effect_{who}')
            for card in ('Grizzly Bears', 'Walking Corpse'):
                self.assertIn(self.copies(card, who, 'hand', 1), result)
            self.assertIn(self.copies('Forest', who, 'graveyard', 1), result)
            outside = self.effect(name, f'outside_{who}')
            self.assertIn(dict(op='assert_zone_card', player=who, zone='library', index=0, value=self.rows['Grizzly Bears']['csvid']), outside)
            self.assertIn(self.copies('Grizzly Bears', who, 'hand', 0), outside)

    def test_typed_union_and_optional_real_decline_both_seats(self):
        for name, alternative in [('Commune with the Gods', 'Fervor'), ('Grisly Salvage', 'Forest'),
                                  ('Scout the Borders', 'Forest')]:
            for who in (0, 1):
                alternate = self.effect(name, f'alternate_{who}')
                self.assertIn(self.copies(alternative, who, 'hand', 2 if alternative == 'Forest' else 1), alternate)
                self.assertIn(dict(op='assert_offered_legal_card', index=self.rows[alternative]['csvid'], value=1), alternate)
                self.assertIn(dict(op='assert_offered_legal_card', index=self.rows['Naturalize']['csvid'], value=0), alternate)
                case = self.case(name, f'decline_{who}')
                used = [s for s in case['steps'] if s['op'] == 'choose_optional_zone']
                expected = 2 if who == 1 and name != 'Commune with the Gods' else 1
                self.assertEqual(len(used), expected)
                self.assertTrue(all(s['player'] == who for s in used))
                self.assertIn(self.copies('Grizzly Bears', who, 'graveyard', 1), self.effect(name, f'decline_{who}'))

    def test_pieces_can_pick_two_one_or_zero_without_expanding_window(self):
        name = 'Pieces of the Puzzle'
        for who in (0, 1):
            for mode, chosen in [('effect', ('Naturalize', 'Divination')), ('one_pick', ('Naturalize',)), ('decline', ())]:
                result = self.effect(name, f'{mode}_{who}')
                for card in ('Naturalize', 'Divination'):
                    self.assertIn(self.copies(card, who, 'hand', int(card in chosen)), result)
                self.assertIn(dict(op='assert_zone_card', player=who, zone='library', index=0, value=self.rows['Shock']['csvid']), result)
                self.assertIn(dict(op='assert_offered_selection_required', value=0), result)
            eligible_once = self.case(name, f'one_available_{who}')['steps']
            self.assertEqual(sum(s['op'] == 'choose_zone_card' for s in eligible_once), 1)
            self.assertEqual(sum(s['op'] == 'choose_optional_zone' for s in eligible_once), 1)

    def test_instant_and_counter_controls_keep_cast_and_payment_boundaries(self):
        for who in (0, 1):
            run = self.case('Grisly Salvage', f'off_turn_{who}')['steps']
            self.assertEqual(run[0], dict(op='turn', player=1 - who, value=0))
            self.assertIn(dict(op='cast', ref='source', value=1), run)
            self.assertIn(dict(op='set_mana', player=who, color=1, value=1), run)
            self.assertIn(dict(op='set_mana', player=who, color=3, value=1), run)
        for name in self.homes:
            short = self.case(name, 'short_mana_0')['steps']
            self.assertIn(dict(op='cast', ref='source', value=0), short)
            self.assertFalse(any(action['op'] == 'resolve' for action in short))
            countered = self.case(name, 'countered_1')['steps']
            self.assertLess(countered.index(dict(op='announce_spell', ref='source')),
                            countered.index(dict(op='cast_response', ref='reply', value=1)))
            self.assertFalse(any(action['op'].startswith('choose_zone') for action in countered))

    def test_replay_reuses_the_real_choices_and_ordered_receipts(self):
        for name in self.homes:
            actions = self.case(name, 'replay_1')['steps']
            cut = actions.index(dict(op='capture'))
            record = actions.index(dict(op='record_state'))
            replay = actions.index(dict(op='restore'))
            self.assertEqual(actions[cut + 1:record], actions[replay + 1:-1])
            self.assertEqual(actions[-1], dict(op='assert_recorded_state'))

    def test_gather_requires_two_own_spells_at_resolution(self):
        name = 'Gather the Pack'
        for who in (0, 1):
            for variant, expected in [('effect', 1), ('mastery_both', 2), ('mastery_nonspell', 1),
                                      ('mastery_foe', 1), ('mastery_same_spell', 2), ('mastery_loses', 1),
                                      ('mastery_small', 1), ('mastery_live', 2)]:
                steps = self.case(name, f'{variant}_{who}')['steps']
                choices = [i for i, step in enumerate(steps) if step['op'] == 'choose_zone_card']
                self.assertEqual(len(choices), expected)
                for choice in choices:
                    self.assertEqual(steps[choice + 1], dict(op='choose_dialog', player=who, value=0))
                if variant != 'mastery_small':
                    self.assertIn(self.copies('Walking Corpse', who, 'hand', int(expected == 2)), steps[choices[-1]:])
            live = self.case(name, f'mastery_live_{who}')['steps']
            prelude = live.index(dict(op='cast', ref='prelude', value=1))
            source = live.index(dict(op='cast', ref='source', value=1))
            self.assertIn(self.copies('Divination', who, 'graveyard', 1), live[prelude:source])
            removed = self.case(name, f'mastery_loses_{who}')['steps']
            response = removed.index(dict(op='activate', ref='crypt', value=1))
            self.assertIn(dict(op='announce_spell', ref='source'), removed[:response])
            self.assertIn(self.copies('Naturalize', who, 'exile', 1), removed[response:])
            self.assertIn(self.copies('Divination', who, 'exile', 1), removed[response:])

    def test_gather_can_end_early_at_mastery(self):
        name = 'Gather the Pack'
        for who in (0, 1):
            for mode, choices, declines in [('mastery_decline', 0, 1), ('mastery_one', 1, 1), ('mastery_small', 1, 0)]:
                steps = self.case(name, f'{mode}_{who}')['steps']
                self.assertEqual(sum(item['op'] == 'choose_zone_card' for item in steps), choices)
                self.assertEqual(sum(item['op'] == 'choose_optional_zone' for item in steps), declines)

    def test_benefaction_selects_distinct_cards_per_type(self):
        name = 'Benefaction of Rhonas'
        for who in (0, 1):
            for variant, chosen in [('dual_creature', (model.DUAL, 'Fervor')), ('dual_enchantment', ('Grizzly Bears', model.DUAL)),
                                    ('dual_once', (model.DUAL,)), ('two_dual', (model.DUAL, model.DUAL)),
                                    ('only_enchantment', ('Fervor',)), ('only_creature', ('Grizzly Bears',)), ('decline', ())]:
                run = self.case(name, f'{variant}_{who}')['steps']
                self.assertEqual([step['value'] for step in run if step['op'] == 'choose_zone_card'],
                                 [self.rows[card]['csvid'] for card in chosen])
                for card in set(chosen):
                    self.assertIn(self.copies(card, who, 'hand', chosen.count(card)), run)
                if variant == 'only_enchantment':
                    self.assertIn(dict(op='assert_offered_count', value=5), run)
                    self.assertIn(dict(op='assert_offered_legal_card', index=self.rows['Fervor']['csvid'], value=1), run)
                if variant == 'dual_once':
                    self.assertEqual(sum(step['op'] == 'choose_optional_zone' for step in run), 1)
