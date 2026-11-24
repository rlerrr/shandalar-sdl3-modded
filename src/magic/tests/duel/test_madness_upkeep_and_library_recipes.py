"""Focused Madness upkeep and library fixture controls."""
import unittest

import madness_upkeep_and_library_recipes as model


class MadnessUpkeepLibraryTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        pins = model.profiles()
        cards = pins['sources'] | pins['witnesses']
        cls.oracle = {name: pin['oracle'] for name, pin in cards.items()}
        cls.rows = {name: pin['registry'] for name, pin in cards.items()}
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
        return [item.get('value') for item in case['steps']
                if item['op'] == op and all(item.get(k) == v for k, v in fields.items())]

    def test_whole_oracle_and_declared_modes(self):
        record = self.oracle[self.names[model.DESCENT]]
        dec = self.declaration(model.DESCENT, 'normal_0')
        for declaration, revised in ((dec | {'version': 2.0}, record),
                                     (dec | {'variants': ['offturn_paid_0']}, record),
                                     (dec, record | {'oracle_text': record['oracle_text'] + '\nDraw a card.'})):
            with self.assertRaises(ValueError):
                model.spec(declaration, revised)

    def test_descent_skip_is_owner_hand_dependent(self):
        for owner in (0, 1):
            empty = self.case(model.DESCENT, f'skip_owner_{owner}')
            live = self.case(model.DESCENT, f'normal_{owner}')
            for c in (empty, live):
                self.assertEqual(self.values(c, 'upkeep_step', player=owner), [None, None])
                self.assertEqual(self.values(c, 'assert_life', player=1-owner)[-1], 18)
            self.assertEqual(self.values(empty, 'assert_life', player=owner)[-1], 20)
            self.assertEqual(self.values(live, 'assert_life', player=owner)[-1], 21)

    def test_visitor_draws_for_empty_active_player_only(self):
        for owner in (0, 1):
            for mode, life, hand in (('normal', 19, 1), ('paid', 18, 3), ('both_full', 20, 1)):
                c = self.case(model.VISITOR, f'{mode}_{owner}')
                self.assertEqual(self.values(c, 'assert_life', player=owner)[-1], life)
                self.assertEqual(self.values(c, 'assert_life', player=1-owner)[-1], 20)
                self.assertEqual(self.values(c, 'assert_zone', player=owner, zone='hand')[-1], hand)

    def test_nagging_exact_top_selection_and_graveyard(self):
        for owner in (0, 1):
            c = self.case(model.THOUGHTS, f'paid_{owner}')
            ids = [self.rows[name]['csvid'] for name in ('Mountain', 'Plains')]
            self.assertEqual(self.values(c, 'assert_offered_card'), ids)
            self.assertEqual(self.values(c, 'choose_zone_card', player=owner), ids[1:])
            self.assertEqual(self.values(c, 'assert_zone_multiplicity', player=owner, zone='graveyard', index=ids[0])[-1], 1)
            self.assertEqual(self.values(c, 'assert_shuffle_count', player=owner), [0])

    def test_offturn_madness_uses_actual_opposing_upkeep_and_cast(self):
        for primary in (model.THOUGHTS, model.TWINS):
            for owner in (0, 1):
                c = self.case(primary, f'offturn_paid_{owner}')
                steps = c['steps']
                activate = next(i for i, item in enumerate(steps) if item['op'] == 'activate')
                self.assertEqual([item['player'] for item in steps[:activate] if item['op'] == 'upkeep_step'],
                                 [1-owner])
                self.assertEqual(self.values(c, 'activate', ref='looter'), [1])

    def test_removal_restores_owner_upkeep_or_stops_visitor(self):
        for owner in (0, 1):
            descent = self.case(model.DESCENT, f'removed_{owner}')
            visitor = self.case(model.VISITOR, f'removed_{owner}')
            self.assertEqual(self.values(descent, 'cast', ref='removal'), [1])
            self.assertEqual(self.values(descent, 'assert_life', player=owner)[-1], 22)
            self.assertEqual(self.values(visitor, 'cast', ref='removal'), [1])
            self.assertEqual(self.values(visitor, 'assert_life', player=owner)[-1], 20)

    def test_countered_uses_paid_response_without_any_library_selection(self):
        for primary in model.CARDS:
            for owner in (0, 1):
                c = self.case(primary, f'normal_countered_{owner}')
                self.assertEqual(self.values(c, 'cast_response', ref='counter'), [1])
                self.assertEqual(self.values(c, 'assert_zone_multiplicity', player=owner, zone='graveyard',
                                             index=primary)[-1], 1)
                self.assertFalse(self.values(c, 'choose_zone_card'))

    def test_twins_resolved_stats_and_real_blocked_damage(self):
        for owner in (0, 1):
            c = self.case(model.TWINS, f'paid_{owner}')
            self.assertEqual(self.values(c, 'assert_card', ref='played', property='toughness'), [5])
            self.assertEqual(self.values(c, 'block', ref='blocker'), [1])
            self.assertEqual(self.values(c, 'assert_card', ref='played', property='damage'), [3])
