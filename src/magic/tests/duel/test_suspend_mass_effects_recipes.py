"""Focused tests for Suspend payment, countdown, and mass outcomes."""
import unittest

import suspend_mass_effects_recipes as model


class SuspendMassEffectTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        pins = model.profiles()
        by_name = pins['sources'] | pins['witnesses']
        cls.oracle = {name: item['oracle'] for name, item in by_name.items()}
        cls.rows = {name: item['registry'] for name, item in by_name.items()}
        cls.names = {row['csvid']: name for name, row in cls.rows.items()}

    def declaration(self, csvid, variant):
        record = self.oracle[self.names[csvid]]
        return dict(family=model.FAMILY, version=2, params=model.classify(record, csvid),
                    variants=[variant], oracle_text=record['oracle_text'])

    def case(self, csvid, variant):
        name = self.names[csvid]
        declaration = self.declaration(csvid, variant)
        shape = model.spec(declaration, self.oracle[name])
        return model.expand(name, csvid, declaration, shape, [variant], self.rows, self.oracle)[0]

    def values(self, case, op, **fields):
        return [action.get('value') for action in case['steps']
                if action['op'] == op and all(action.get(k) == v for k, v in fields.items())]

    def test_complete_oracle_and_typed_recipe(self):
        record = self.oracle[self.names[model.WHEEL]]
        d = self.declaration(model.WHEEL, 'countdown_0')
        for decl, actual in ((d | {'version': 2.0}, record), (d | {'variants': ['returned_owner_0']}, record),
                             (d, record | {'oracle_text': record['oracle_text'] + '\nDraw a card.'})):
            with self.assertRaises(ValueError):
                model.spec(decl, actual)

    def test_actual_upkeep_counts_and_early_nonrelease(self):
        for csvid, (count, _, _, _) in model.CARDS.items():
            for owner in (0, 1):
                for mode, expected in (('parked', [1-owner]), ('first_owner', [1-owner, owner]),
                                       ('countdown', [1-owner, owner] * count)):
                    case = self.case(csvid, f'{mode}_{owner}')
                    self.assertEqual([step['player'] for step in case['steps'] if step['op'] == 'upkeep_step'],
                                     expected)

    def test_wrong_color_stays_in_hand_without_payment(self):
        for csvid in model.CARDS:
            for owner in (0, 1):
                c = self.case(csvid, f'wrong_color_{owner}')
                required = self.declaration(csvid, f'wrong_color_{owner}')['params']['mana']
                remaining = self.values(c, 'assert_mana', player=owner)
                self.assertEqual(sum(remaining), sum(required))
                self.assertNotEqual(remaining, required)
                self.assertEqual(self.values(c, 'assert_zone_multiplicity', player=owner, zone='hand',
                                             index=csvid), [1])
                self.assertEqual(self.values(c, 'assert_zone_multiplicity', player=owner, zone='exile',
                                             index=csvid), [0])

    def test_dichotomancy_only_shuffles_opponent_once(self):
        for owner in (0, 1):
            for mode in ('countdown', 'returned_owner', 'search_empty'):
                c = self.case(model.DICHO, f'{mode}_{owner}')
                self.assertEqual(self.values(c, 'assert_shuffle_count', player=1-owner), [1])
                self.assertEqual(self.values(c, 'assert_shuffle_count', player=owner), [0])
                self.assertEqual(self.values(c, 'activate', ref='land'), [] if mode == 'search_empty' else [1])

    def test_hypergenesis_picks_continue_despite_one_player_declining(self):
        for owner in (0, 1):
            c = self.case(model.HYPER, f'one_declines_{owner}')
            self.assertEqual(self.values(c, 'choose_zone_card', player=owner),
                             [self.rows['Grizzly Bears']['csvid'], self.rows['Glorious Anthem']['csvid']])
            self.assertEqual(len(self.values(c, 'choose_optional_zone', player=1-owner)), 3)
            self.assertEqual(self.values(c, 'assert_zone', player=owner, zone='battlefield')[-1], 2)

    def test_living_and_balance_use_separate_zones(self):
        for owner in (0, 1):
            c = self.case(model.LIVING, f'countdown_{owner}')
            self.assertEqual(self.values(c, 'assert_zone_multiplicity', player=owner, zone='battlefield',
                                         index=self.rows['Hill Giant']['csvid'])[-1], 1)
            self.assertEqual(self.values(c, 'assert_zone_multiplicity', player=owner, zone='graveyard',
                                         index=self.rows['Grizzly Bears']['csvid'])[-1], 1)
            c = self.case(model.BALANCE, f'choices_{owner}')
            self.assertEqual(self.values(c, 'assert_zone', player=0, zone='hand')[-1], 1)
            self.assertEqual(self.values(c, 'assert_zone', player=1, zone='hand')[-1], 1)

    def test_wheel_keeps_exact_draw_and_discard_receipts(self):
        for owner in (0, 1):
            c = self.case(model.WHEEL, f'countdown_{owner}')
            self.assertEqual(self.values(c, 'discard_policy'), [1])
            for seat in (0, 1):
                self.assertEqual(self.values(c, 'assert_zone', player=seat, zone='hand')[-1], 7)
                self.assertEqual(self.values(c, 'assert_zone', player=seat, zone='library')[-1], 1)
            self.assertEqual(self.values(c, 'assert_zone', player=owner, zone='graveyard')[-1], 3)
            self.assertEqual(self.values(c, 'assert_zone', player=1-owner, zone='graveyard')[-1], 1)

    def test_replay_repeats_real_casts_and_upkeeps(self):
        for csvid in model.REPLAY:
            for owner in (0, 1):
                acts = self.case(csvid, f'replay_{owner}')['steps']
                ops = [act['op'] for act in acts]
                self.assertEqual(acts[ops.index('capture')+1:ops.index('record_state')],
                                 acts[ops.index('restore')+1:ops.index('assert_recorded_state')])
