"""Focused source, threshold, and scenario guards for global effects."""
import unittest

import threshold_global_effects_recipes as model


class ThresholdGlobalEffectTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        pins = model.profiles()
        records = pins['sources'] | pins['witnesses']
        cls.oracle = {name: item['oracle'] for name, item in records.items()}
        cls.rows = {name: item['registry'] for name, item in records.items()} | pins['token_registry']
        cls.names = {record['csvid']: name for name, record in cls.rows.items()}

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

    def test_pinned_oracle_and_scope_fail_closed(self):
        record = self.oracle[self.names[model.NUT]]
        decl = self.declaration(model.NUT, 'decline_0')
        for actual, oracle in ((decl | {'version': 2.0}, record), (decl | {'variants': ['decline_1']}, record),
                               (decl, record | {'oracle_text': record['oracle_text'] + '\nDraw a card.'})):
            with self.assertRaises(ValueError):
                model.spec(actual, oracle)

    def test_duplicates_and_graveyard_owner_scope(self):
        for csvid in model.CLAUSES:
            for owner in (0, 1):
                for variant, expected in (('six', (6, 0)), ('seven', (7, 0)), ('opponent_only', (6, 7))):
                    case = self.case(csvid, f'{variant}_{owner}')
                    grave = [item for item in case['setup'] if item['zone'] == 'graveyard']
                    self.assertTrue(all(item['name'] == 'Mountain' for item in grave))
                    self.assertEqual(tuple(sum(item['player'] == seat for item in grave)
                                           for seat in (owner, 1-owner)), expected)

    def test_shower_zero_and_one_use_cancel_and_keep_unselected_targets(self):
        for owner in (0, 1):
            for mode, life, selections in (('one_target', 16, [1-owner]), ('zero_targets', 20, [])):
                case = self.case(model.COALS, f'{mode}_{owner}')
                self.assertEqual(self.values(case, 'choose_player', player=owner), selections)
                self.assertEqual(self.values(case, 'choose_cancel_target', player=owner), [None])
                self.assertEqual(self.values(case, 'choose_done_target'), [])
                self.assertEqual(self.values(case, 'assert_life', player=1-owner), [life])
                for ref in ('durable', 'fragile', 'foil'):
                    self.assertEqual(self.values(case, 'assert_card', ref=ref, property='damage'), [0])

    def test_funding_variants_use_real_insufficient_casts(self):
        for csvid in model.CLAUSES:
            for owner in (0, 1):
                full = sum(self.declaration(csvid, f'short_{owner}')['params']['mana'])
                for mode, expected in (('short', full - 1), ('wrong_color', full)):
                    case = self.case(csvid, f'{mode}_{owner}')
                    self.assertEqual(self.values(case, 'cast', ref='source'), [0])
                    self.assertEqual(sum(self.values(case, 'assert_mana', player=owner)), expected)
                    self.assertEqual(self.values(case, 'assert_zone_multiplicity', index=csvid, zone='hand'), [1])

    def test_countered_announces_actual_parent_and_reply(self):
        for csvid in model.CLAUSES:
            for owner in (0, 1):
                case = self.case(csvid, f'countered_{owner}')
                operations = [action['op'] for action in case['steps']]
                self.assertLess(operations.index('cast'), operations.index('announce_spell'))
                self.assertLess(operations.index('announce_spell'), operations.index('cast_response'))
                self.assertLess(operations.index('cast_response'), operations.index('resolve_response'))
                self.assertEqual(self.values(case, 'assert_zone_multiplicity', index=csvid, zone='graveyard'), [1])

    def test_seventh_card_from_actual_paid_spell(self):
        for csvid in model.CLAUSES:
            for owner in (0, 1):
                case = self.case(csvid, f'gain_seventh_{owner}')
                refs = [action['ref'] for action in case['steps'] if action['op'] == 'cast']
                self.assertEqual(refs[:2], ['source', 'shock'] if csvid == model.NUT else ['shock', 'source'])
                self.assertEqual(self.values(case, 'assert_zone', player=owner, zone='graveyard'), [6, 7])

    def test_nut_lord_stops_at_real_graveyard_exile(self):
        for owner in (0, 1):
            case = self.case(model.NUT, f'lose_seventh_{owner}')
            self.assertEqual(self.values(case, 'activate', ref='crypt'), [1])
            self.assertEqual(self.values(case, 'assert_zone', player=owner, zone='graveyard'), [7, 0])
            for ref in ('own_changeling', 'foe_changeling'):
                self.assertEqual(self.values(case, 'assert_card', ref=ref, property='power'), [4, 2, 2, 2])
            self.assertEqual(self.values(case, 'assert_matching_permanents', property='power'), [1])

    def test_replay_reuses_real_paid_actions_and_receipts(self):
        for csvid in model.REPLAY:
            for owner in (0, 1):
                actions = self.case(csvid, f'replay_{owner}')['steps']
                ops = [action['op'] for action in actions]
                self.assertEqual(actions[ops.index('capture') + 1:ops.index('record_state')],
                                 actions[ops.index('restore') + 1:ops.index('assert_recorded_state')])
