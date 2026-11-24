"""Guard the whole Flash records and the new matched-color/expiry cases."""

import unittest

import flash_entry_spell_access_and_protection_recipes as model


class FlashSpellAccessGuard(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        pins = model.profiles()["sources"] | model.profiles()["witnesses"]
        cls.oracle = {name: pin["oracle"] for name, pin in pins.items()}
        cls.rows = {name: pin["registry"] for name, pin in pins.items()}

    def tiger(self, mode, seat):
        board = model.Board(model.TIGER, "Seht's Tiger", mode, seat, self.rows, self.oracle)
        board.run()
        return board

    def test_exact_whole_record_and_recipe_version(self):
        model.validate_witnesses(self.oracle, self.rows)
        for name, source in model.profiles()["sources"].items():
            record = self.oracle[name]
            shape = model.classify(record, source["registry"]["csvid"])
            declaration = dict(family=model.FAMILY, version=2, params=shape,
                               oracle_text=record["oracle_text"], variants=model.variants(shape))
            self.assertEqual(model.spec(declaration, record)["csvid"], shape["csvid"])
            with self.assertRaises(ValueError):
                model.spec(declaration, {**record, "oracle_text": record["oracle_text"] + "\nDraw a card."})
            with self.assertRaises(ValueError):
                model.spec({**declaration, "version": True}, record)
            with self.assertRaises(ValueError):
                model.spec({**declaration, "variants": ["invalid_0"]}, record)

    def test_matching_color_rejects_target_and_prevents_untargeted_damage(self):
        for seat in (0, 1):
            targeted = self.tiger("targeted_fizzle", seat)
            rift = self.tiger("untargeted_prevention", seat)
            self.assertEqual(targeted.objects["parent"]["name"], model.BOLT)
            self.assertEqual(rift.objects["parent"]["name"], model.RIFT)
            self.assertEqual((targeted.life[seat], targeted.life[1 - seat]), (17, 20))
            self.assertEqual((rift.life[seat], rift.life[1 - seat]), (17, 16))
            for board in (targeted, rift):
                actions = [s["op"] for s in board.steps]
                self.assertLess(actions.index("cast_response"), actions.index("choose_color"))
                self.assertLess(actions.index("choose_color"), actions.index("resolve_response"))
                self.assertLess(actions.index("cleanup"), actions.index("advance_turn"))
                self.assertEqual([s["value"] for s in board.steps if s["op"] == "can_cast"
                                  and s.get("ref") == "red_probe"], [1, 0, 1])

    def test_blue_choice_leaves_red_damage_and_targeting_legal(self):
        for seat in (0, 1):
            board = self.tiger("different_color", seat)
            self.assertEqual((board.life[seat], board.life[1 - seat]), (13, 16))
            self.assertEqual([s["value"] for s in board.steps if s["op"] == "choose_color"], [model.BLUE])
            self.assertEqual([s["value"] for s in board.steps if s["op"] == "can_cast"
                              and s.get("ref") == "blue_probe"], [1, 0, 1])

    def test_red_can_kill_tiger_without_ending_player_protection(self):
        for seat in (0, 1):
            board = self.tiger("source_removed", seat)
            self.assertEqual(board.objects["source"]["zone"], "graveyard")
            self.assertEqual((board.life[seat], board.life[1 - seat]), (17, 12))
            self.assertEqual([s["ref"] for s in board.steps if s["op"] == "cast"],
                             ["parent", "red_removal", "continued_damage", "after_expiry"])
            self.assertEqual([s["value"] for s in board.steps if s["op"] == "can_cast"
                              and s.get("ref") == "red_probe"], [1, 0, 0, 1])
