"""Pin whole Equipment cards and check independent attack outcomes."""

import unittest

import equipment_combat_attack_land_and_library_recipes as model


class EquipmentLibraryGuard(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        profiles = model.profiles()
        pins = profiles['sources'] | profiles['witnesses']
        cls.oracle = {name: pin['oracle'] for name, pin in pins.items()}
        cls.rows = {name: pin['registry'] for name, pin in pins.items()}
        cls.by_id = {row['csvid']: row for row in cls.rows.values()}

    def game(self, csvid, mode, actor=0):
        game = model.Duel(self.by_id[csvid], actor, mode, self.rows, self.oracle)
        game.build()
        return game

    def test_complete_rules_and_exact_schema(self):
        model.validate_witnesses(self.oracle, self.rows)
        for csvid in model.CARDS:
            record = self.oracle[self.by_id[csvid]['name']]
            shape = model.classify(record, csvid)
            declaration = dict(family=model.FAMILY, version=2, params=shape,
                               oracle_text=record['oracle_text'], variants=model.variants(shape))
            self.assertEqual(model.spec(declaration, record)['csvid'], csvid)
            with self.assertRaises(ValueError):
                model.spec(declaration, {**record, 'oracle_text': record['oracle_text'] + '\nDraw a card.'})
            with self.assertRaises(ValueError):
                model.spec({**declaration, 'version': True}, record)

    def test_blade_reveals_only_through_first_land_and_pump_expires(self):
        for actor in (0, 1):
            game = self.game(12104, 'deep_land', actor)
            self.assertEqual([game.cards[ref]['name'] for ref in game.library[1 - actor]], ['Plains', 'Hill Giant'])
            self.assertEqual([s['value'] for s in game.steps if s['op'] == 'assert_offered_count'], [3])
            self.assertEqual([s['value'] for s in game.steps if s['op'] == 'assert_card'
                              and s['ref'] == 'host' and s['property'] == 'power'], [2, 2, 5, 5, 2])

    def test_scope_and_sword_keep_different_land_and_shuffle_rules(self):
        scope = self.game(10538, 'take_land')
        sword = self.game(15229, 'search_basic')
        fail = self.game(15229, 'fail_to_find')
        self.assertEqual((scope.cards[scope.bound_land]['name'], scope.shuffles), (model.NONBASIC, [0, 0]))
        self.assertEqual((sword.cards[sword.bound_land]['name'], sword.shuffles), (model.FETCH, [1, 0]))
        self.assertIsNone(fail.bound_land)
        self.assertEqual(fail.shuffles, [1, 0])

    def test_slab_owner_hand_decline_and_combat_bonus(self):
        for actor in (0, 1):
            borrowed = self.game(15444, 'return_borrowed', actor)
            declined = self.game(15444, 'decline', actor)
            self.assertIn('opponent_owned', next(card['state'] for card in borrowed.setup if card['ref'] == 'borrowed'))
            self.assertEqual((borrowed.cards['borrowed']['player'], borrowed.cards['borrowed']['zone']), (1 - actor, 'hand'))
            self.assertEqual(borrowed.life[1 - actor], 16)
            self.assertEqual([s['value'] for s in borrowed.steps if s['op'] == 'assert_card'
                              and s['ref'] == 'host' and s['property'] == 'toughness'], [2, 2, 4, 4, 2])
            self.assertEqual(declined.cards['own_land']['zone'], 'battlefield')
            self.assertEqual(declined.life[1 - actor], 18)
            self.assertTrue(any(s['op'] == 'choose_optional_target' for s in declined.steps))
