"""Verify exact new mill receipts and paid lifecycle boundaries."""

import unittest

import mill_death_and_entry_recipes as model


class MillDeathAndEntryGuard(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        profiles = model.profiles()
        pins = profiles['sources'] | profiles['witnesses']
        cls.oracle = {name: pin['oracle'] for name, pin in pins.items()}
        cls.rows = {name: pin['registry'] for name, pin in pins.items()}
        cls.records = {p['registry']['csvid']: p['oracle'] for p in profiles['sources'].values()}

    def game(self, csvid, mode, seat):
        model_class = model.Duel if csvid == model.SUPPLIER else model.EntryFollowOn
        game = model_class(csvid, self.records[csvid], seat, mode, self.rows, self.oracle)
        game.build()
        return game

    def test_complete_rule_and_fees_are_required(self):
        record = self.records[model.GLOWSPORE]
        shape = model.classify(record, model.GLOWSPORE)
        decl = dict(family=model.FAMILY, version=2, params=shape,
                    oracle_text=record['oracle_text'], variants=['recover_milled_0', 'no_land_1'])
        self.assertEqual(model.spec(decl, record)['mana'], [0, 1, 0, 1, 0, 0])
        with self.assertRaises(ValueError):
            model.spec(decl, dict(record, oracle_text=record['oracle_text'].replace('land card', 'card')))
        with self.assertRaises(ValueError):
            model.spec(dict(decl, variants=['unreviewed_0']), record)
        with self.assertRaises(ValueError):
            model.spec(dict(decl, version=2.0), record)

    def test_denizen_requires_another_friendly_blue_creature(self):
        for seat in (0, 1):
            for mode, target in (('blue', 1 - seat), ('multicolor', seat)):
                game = self.game(model.DENIZEN, mode, seat)
                choices = [step['value'] for step in game.steps if step['op'] == 'choose_player']
                self.assertEqual(choices, [target])
                starting = model.MY_LIBRARY if target == seat else model.FOE_LIBRARY
                self.assertEqual(game.mill_events, [dict(player=target, requested=2, cards=starting[:2])])
            for mode in ('foe_blue', 'nonblue', 'noncreature'):
                game = self.game(model.DENIZEN, mode, seat)
                self.assertEqual(game.mill_events, [])
                self.assertFalse(any(s['op'] == 'choose_player' for s in game.steps))

    def test_supplier_mills_three_each_at_entry_and_its_actual_death(self):
        for seat in (0, 1):
            for mode, moved in (('enter_die', [3, 3]), ('short', [3, 1]), ('reentry', [3, 3]),
                                ('exile', [3]), ('other_dies', [3])):
                game = self.game(model.SUPPLIER, mode, seat)
                self.assertEqual([len(event['cards']) for event in game.mill_events], moved)
                self.assertTrue(all(event['requested'] == 3 and event['player'] == seat for event in game.mill_events))
            again = self.game(model.SUPPLIER, 'reentry', seat)
            self.assertIn(dict(op='cast', ref='returned', value=1), again.steps)

    def test_glowspore_can_choose_new_or_old_land_or_decline(self):
        for seat in (0, 1):
            for mode, top in (('recover_milled', 'Forest'), ('recover_old', 'Wastes'), ('short', 'Island')):
                game = self.game(model.GLOWSPORE, mode, seat)
                self.assertEqual(game.library[seat][0], top)
                self.assertIn(dict(op='choose_zone_card', player=seat, value=self.rows[top]['csvid']), game.steps)
            decline = self.game(model.GLOWSPORE, 'decline', seat)
            self.assertEqual(decline.library[seat], model.MY_LIBRARY[3:])
            self.assertIn(dict(op='choose_optional_zone', player=seat, value=self.rows['Forest']['csvid']), decline.steps)
            none = self.game(model.GLOWSPORE, 'no_land', seat)
            self.assertFalse(any(s['op'].startswith('choose_') for s in none.steps))

    def test_paid_gravestone_does_not_prevent_nontargeted_recovery(self):
        for seat in (0, 1):
            game = self.game(model.GLOWSPORE, 'grave_shroud', seat)
            cast_seal = game.steps.index(dict(op='cast', ref='seal', value=1))
            cast_source = game.steps.index(dict(op='cast', ref='source', value=1))
            self.assertLess(cast_seal, cast_source)
            self.assertEqual(game.cards['seal']['zone'], 'battlefield')
            self.assertEqual(game.library[seat][0], 'Forest')

    def test_wall_targets_either_player_and_uses_real_defender_check(self):
        for seat in (0, 1):
            for mode, target, count in (('foe', 1 - seat, 4), ('self', seat, 4), ('short', 1 - seat, 1)):
                game = self.game(model.WALL, mode, seat)
                event, = game.mill_events
                self.assertEqual((event['player'], event['requested'], len(event['cards'])), (target, 4, count))
            game = self.game(model.WALL, 'defender', seat)
            self.assertIn(dict(op='attack', ref='source', value=0), game.steps)
            self.assertIn(dict(op='attack', ref='attacker', value=1), game.steps)
            self.assertEqual(game.life[1 - seat], 19)
