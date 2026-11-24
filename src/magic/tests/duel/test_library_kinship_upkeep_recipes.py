"""Guard the pinned Kinship scopes and corroborate offered choice ordering."""

import unittest

import library_kinship_upkeep_recipes as model


class KinshipGuard(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        pins = model.profiles()["sources"] | model.profiles()["witnesses"]
        cls.oracle = {name: pin["oracle"] for name, pin in pins.items()}
        cls.rows = {name: pin["registry"] for name, pin in pins.items()}
        cls.names = {pin["registry"]["csvid"]: name for name, pin in pins.items()}

    def board(self, csvid, mode, seat):
        game = model.Board(csvid, self.names[csvid], mode, seat, self.rows, self.oracle)
        game.run()
        return game

    def test_whole_record_identity_and_actor_scope(self):
        model.validate_witnesses(self.oracle, self.rows)
        for name, pin in model.profiles()["sources"].items():
            record = self.oracle[name]
            shape = model.classify(record, pin["registry"]["csvid"])
            decl = dict(family=model.FAMILY, version=2, params=shape,
                        oracle_text=record["oracle_text"], variants=model.variants(shape))
            self.assertEqual(model.spec(decl, record)["csvid"], shape["csvid"])
            self.assertNotIn("decline_reveal_1", decl["variants"])
            with self.assertRaises(ValueError):
                model.spec(decl, {**record, "oracle_text": record["oracle_text"] + " Draw a card."})

    def test_prompt_order_matches_observed_reveal_notice_and_play(self):
        def dialogs(steps):
            return [(s.get("player", 0), s["value"]) for s in steps if s["op"] == "choose_dialog"]

        self.assertEqual(dialogs(self.board(model.ZEPHYR, "nonmatch", 0).steps), [(0, 0)])
        self.assertEqual(dialogs(self.board(model.ZEPHYR, "nonmatch", 1).steps), [])
        self.assertEqual(dialogs(self.board(model.ZEPHYR, "decline_reveal", 0).steps), [(0, 1)])
        self.assertEqual(dialogs(self.board(model.ELDER, "pass_after_reveal", 1).steps), [(1, 0), (1, 1)])
        self.assertEqual(dialogs(self.board(model.ELDER, "cast_kindred", 0).steps), [(0, 0), (0, 0)])

    def test_followers_keep_exact_beneficiaries_and_generated_token(self):
        for owner in (0, 1):
            foe = 1 - owner
            game = self.board(model.GRUB, "opponent_discards", owner)
            self.assertEqual(game.objects["opponent_hand"]["player"], foe)
            self.assertEqual(game.objects["opponent_hand"]["zone"], "graveyard")
            self.assertEqual(game.objects["own_hand"]["zone"], "hand")
            self.assertEqual(self.board(model.GRAY, "gain_four", owner).life[owner], 24)
            self.assertEqual(self.board(model.GRAY, "gain_four", owner).life[foe], 20)
            wolf = self.board(model.WOLF_SHAMAN, "create_wolf", owner)
            self.assertNotIn(model.WOLF, {obj["name"] for obj in wolf.objects.values()})
            observed = {s["property"]: s["value"] for s in wolf.steps if s["op"] == "assert_matching_permanents"}
            self.assertEqual(observed, dict(power=2, toughness=2, token=1, color=8, subtype_Wolf=1))
            self.assertFalse(any(s["op"] == "assert_matching_permanents"
                                 for s in self.board(model.WOLF_SHAMAN, "nonmatch", owner).steps))
