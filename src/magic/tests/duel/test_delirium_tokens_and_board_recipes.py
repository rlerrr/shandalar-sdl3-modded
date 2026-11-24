"""Focused guards for Delirium conditions and real fixture sequencing."""
import unittest

import delirium_tokens_and_board_recipes as model


class DeliriumBoardTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        pins = model.profiles()
        cards = pins['sources'] | pins['witnesses']
        cls.oracle = {name: pin['oracle'] for name, pin in cards.items()}
        cls.rows = {name: pin['registry'] for name, pin in cards.items()} | pins['token_registry']
        cls.names = {pin['csvid']: name for name, pin in cls.rows.items()}

    def declaration(self, csvid, variant):
        record = self.oracle[self.names[csvid]]
        return dict(family=model.FAMILY, version=2, params=model.classify(record, csvid),
                    variants=[variant], oracle_text=record['oracle_text'])

    def case(self, csvid, variant):
        name = self.names[csvid]
        declaration = self.declaration(csvid, variant)
        shape = model.spec(declaration, self.oracle[name])
        return model.expand(name, csvid, declaration, shape, [variant], self.rows, self.oracle)[0]

    def matching(self, case, op, **fields):
        return [s for s in case['steps'] if s['op'] == op and all(s.get(k) == v for k, v in fields.items())]

    def values(self, case, op, **fields):
        return [s.get('value') for s in self.matching(case, op, **fields)]

    def test_record_and_declaration_fail_closed(self):
        record = self.oracle[self.names[model.DESCEND]]
        declaration = self.declaration(model.DESCEND, 'four_types_0')
        for d, r in (
            (declaration, record | {'oracle_text': record['oracle_text'] + '\nDraw a card.'}),
            (declaration | {'version': 2.0}, record),
            (declaration | {'params': declaration['params'] | {'csvid': float(model.DESCEND)}}, record),
            (declaration | {'variants': ['repeat_upkeep_0']}, record),
        ):
            with self.assertRaises(ValueError):
                model.spec(d, r)

    def test_equal_grave_sizes_and_owner_scope(self):
        for csvid in model.CLAUSES:
            for owner in (0, 1):
                for mode, expected in (('three_duplicates', (3, 0)), ('four_types', (4, 0)),
                                       ('opponent_only', (3, 4))):
                    c = self.case(csvid, f'{mode}_{owner}')
                    own = [r['name'] for r in c['setup'] if r['zone'] == 'graveyard' and r['player'] == owner]
                    foe = [r['name'] for r in c['setup'] if r['zone'] == 'graveyard' and r['player'] != owner]
                    self.assertEqual(len(own), 4)
                    self.assertEqual((len(model.types_for(own, self.oracle)), len(model.types_for(foe, self.oracle))),
                                     expected)

    def test_short_and_wrong_color_are_distinct_real_casts(self):
        for csvid in model.CLAUSES:
            for owner in (0, 1):
                total = sum(self.declaration(csvid, f'short_{owner}')['params']['mana'])
                for mode, expected in (('short', total - 1), ('wrong_color', total)):
                    c = self.case(csvid, f'{mode}_{owner}')
                    self.assertEqual(self.values(c, 'cast', ref='source'), [0])
                    self.assertEqual(sum(self.values(c, 'assert_mana', player=owner)), expected)
                    self.assertEqual(self.values(c, 'assert_zone_multiplicity', zone='hand', index=csvid), [1])

    def test_countered_uses_real_parent_and_reply(self):
        for csvid in model.CLAUSES:
            for owner in (0, 1):
                c = self.case(csvid, f'countered_{owner}')
                ops = [s['op'] for s in c['steps']]
                self.assertLess(ops.index('cast'), ops.index('announce_spell'))
                self.assertLess(ops.index('announce_spell'), ops.index('cast_response'))
                self.assertLess(ops.index('cast_response'), ops.index('resolve_response'))
                self.assertEqual(self.values(c, 'assert_zone_multiplicity', zone='battlefield', index=csvid), [0])
                self.assertEqual(self.values(c, 'assert_zone_multiplicity', zone='graveyard', index=csvid), [1])

    def test_fourth_type_from_paid_disenchant_in_real_order(self):
        amulet = self.rows["Traveler's Amulet"]['csvid']
        for csvid in model.CLAUSES:
            for owner in (0, 1):
                c = self.case(csvid, f'gain_fourth_{owner}')
                casts = [s['ref'] for s in self.matching(c, 'cast')]
                before_source = csvid in (model.DESCEND, model.ISHKANAH, model.MOURN)
                self.assertEqual(casts[:2], ['disenchant', 'source'] if before_source else ['source', 'disenchant'])
                self.assertEqual(self.values(c, 'assert_zone_multiplicity', index=amulet, zone='graveyard',
                                             player=owner), [0, 1])
                if csvid == model.SENTRY:
                    self.assertEqual(self.values(c, 'assert_card', ref='source', property='power'), [1, 4])

    def test_replay_exactly_repeats_actual_actions(self):
        for csvid in model.REPLAY:
            for owner in (0, 1):
                c = self.case(csvid, f'replay_{owner}')
                cases = c['steps']
                ops = [s['op'] for s in cases]
                self.assertEqual(cases[ops.index('capture') + 1:ops.index('record_state')],
                                 cases[ops.index('restore') + 1:ops.index('assert_recorded_state')])
                self.assertEqual(self.values(c, 'cast', ref='source'), [1, 1])

    def test_combat_and_life_use_real_affected_recipients(self):
        for owner in (0, 1):
            blob = self.case(model.BLOB, f'four_types_{owner}')
            self.assertEqual(self.values(blob, 'assert_matching_permanents', property='attacking'), [1, 0])
            self.assertEqual(self.values(blob, 'assert_life', player=1-owner), [14])
            ishkanah = self.case(model.ISHKANAH, f'four_types_{owner}')
            self.assertEqual(self.values(ishkanah, 'activate', ref='source'), [1])
            self.assertEqual(self.values(ishkanah, 'assert_life', player=1-owner), [20, 15])
            expiry = self.case(model.MOURN, f'expires_{owner}')
            self.assertEqual(self.values(expiry, 'block', ref='small'), [0, 1])

    def test_token_identity_requires_live_registered_pin(self):
        altered = dict(self.rows)
        altered['Spider'] = altered['Spider'] | {'csvid': -1}
        with self.assertRaises(ValueError):
            model.validate_witnesses(self.oracle, altered)
