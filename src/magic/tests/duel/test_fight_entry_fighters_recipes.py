"""Focused validation of complete entry-fight records and Cartouche actions."""

import unittest

import fight_entry_fighters_recipes as model


class EntryFightGuard(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        pins = model.profiles()
        merged = pins['sources'] | pins['witnesses']
        cls.oracle = {name: record['oracle'] for name, record in merged.items()}
        cls.rows = {name: record['registry'] for name, record in merged.items()}
        cls.record = cls.oracle[model.CARTOUCHE_NAME]

    def aura(self, mode, seat):
        board = model.AuraFight(model.CARTOUCHE, self.record, seat, mode, self.rows, self.oracle)
        board.build()
        return board

    def test_whole_record_and_legal_variant_gates_fail_closed(self):
        shape = model.classify(self.record, model.CARTOUCHE)
        declaration = dict(family=model.FAMILY, version=2, params=shape,
                           oracle_text=self.record['oracle_text'], variants=['decline_0', 'trample_1'])
        self.assertEqual(model.spec(declaration, self.record)['mana'], [2, 0, 0, 1, 0, 0])
        with self.assertRaises(ValueError):
            model.spec(declaration, {**self.record, 'oracle_text': self.record['oracle_text'].replace(' and has trample', '')})
        with self.assertRaises(ValueError):
            model.spec({**declaration, 'variants': ['decline_1']}, self.record)
        with self.assertRaises(ValueError):
            model.spec({**declaration, 'version': 2.0}, self.record)
        with self.assertRaises(ValueError):
            model.validate_witnesses({**self.oracle, model.HOST: {'power': '2'}}, self.rows)

    def test_cast_target_and_fight_target_are_distinct_and_timed(self):
        for seat in (0, 1):
            game = self.aura('reciprocal', seat)
            attach = game.steps.index(dict(op='choose_target', ref='host', player=seat))
            cast = game.steps.index(dict(op='cast', ref='source', value=1))
            fight = game.steps.index(dict(op='choose_target', ref='victim', player=seat))
            resolve = game.steps.index(dict(op='resolve'))
            self.assertLess(attach, cast)
            self.assertLess(cast, fight)
            self.assertLess(fight, resolve)
            self.assertEqual(game.damage['host'], 2)
            self.assertEqual(game.cards['victim']['zone'], 'graveyard')

    def test_human_can_decline_and_foreign_creature_cannot_anchor_cast(self):
        decline = self.aura('decline', 0)
        self.assertFalse(any(s['op'] == 'choose_trigger' or s.get('ref') == 'victim'
                             for s in decline.steps if s['op'].startswith('choose')))
        self.assertEqual(decline.cards['source']['zone'], 'battlefield')
        self.assertEqual(decline.damage, {})
        for seat in (0, 1):
            game = self.aura('foreign_only', seat)
            self.assertIn(dict(op='can_cast', ref='source', value=0), game.steps)
            self.assertNotIn('host', game.cards)
            self.assertEqual(game.cards['source']['zone'], 'hand')

    def test_real_response_prevents_entry_and_lethal_fight_detaches_aura(self):
        for seat in (0, 1):
            missed = self.aura('lost_host', seat)
            ops = [s['op'] for s in missed.steps]
            self.assertLess(ops.index('announce_spell'), ops.index('cast_response'))
            self.assertLess(ops.index('cast_response'), ops.index('resolve_response'))
            self.assertEqual(missed.cards['host']['zone'], 'hand')
            self.assertEqual(missed.cards['source']['zone'], 'graveyard')
            self.assertEqual(missed.cards['victim']['zone'], 'battlefield')
            self.assertEqual(missed.damage, {})
            lethal = self.aura('lethal_fight', seat)
            self.assertEqual(lethal.cards['host']['zone'], 'graveyard')
            self.assertEqual(lethal.cards['source']['zone'], 'graveyard')
            self.assertEqual(lethal.damage['victim'], 2)

    def test_only_actual_combat_tramples_past_hexproof_blocker(self):
        for seat in (0, 1):
            game = self.aura('trample', seat)
            self.assertNotIn(dict(op='choose_target', ref='victim', player=seat), game.steps)
            self.assertIn(dict(op='block', ref='victim', other='host', value=1), game.steps)
            self.assertIn(dict(op='combat_damage'), game.steps)
            self.assertEqual(game.life[1-seat], 19)
            self.assertEqual(game.cards['victim']['zone'], 'graveyard')
