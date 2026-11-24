"""Guard the eight pinned primaries and the new mill trigger boundaries."""

import unittest

import mill_remaining_phase_and_mill_triggers_recipes as model


class MillPhaseAndTriggerGuard(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        pins = model.profiles()["sources"] | model.profiles()["witnesses"]
        cls.oracle = {name: pin["oracle"] for name, pin in pins.items()}
        cls.rows = {name: pin["registry"] for name, pin in pins.items()}
        cls.names = {pin["registry"]["csvid"]: name for name, pin in pins.items()}

    def board(self, csvid, mode, seat):
        board = model.Board(csvid, self.names[csvid], mode, seat, self.rows, self.oracle)
        board.run()
        return board

    def test_whole_oracle_and_actor_scope(self):
        model.validate_witnesses(self.oracle, self.rows)
        for name, pin in model.profiles()["sources"].items():
            record = self.oracle[name]
            shape = model.classify(record, pin["registry"]["csvid"])
            declaration = dict(family=model.FAMILY, version=2, params=shape,
                               oracle_text=record["oracle_text"], variants=model.variants(shape))
            self.assertEqual(model.spec(declaration, record)["csvid"], shape["csvid"])
            with self.assertRaises(ValueError):
                model.spec(declaration, {**record, "oracle_text": record["oracle_text"] + "\nDraw a card."})
        self.assertNotIn("decline_reveal_1", model.variants(dict(csvid=model.INK)))

    def test_crawling_observes_one_event_per_turn_and_both_owners(self):
        token = self.rows[model.INSECT]["csvid"]
        for seat in (0, 1):
            for mode, expected in (("upkeep_and_external", [1, 1, 2]), ("own_land_only", [0, 0, 1])):
                board = self.board(model.CRAWL, mode, seat)
                counts = [s["value"] for s in board.steps
                          if s["op"] == "assert_zone_multiplicity" and s.get("index") == token
                          and s.get("player", 0) == seat]
                self.assertEqual(counts, expected)
                self.assertEqual(board.objects["own_land"]["zone"], "graveyard")

    def test_sewer_recounts_at_announcement_before_the_original_draw(self):
        for seat in (0, 1):
            own = self.board(model.SEWER, "choose_self", seat)
            foe = self.board(model.SEWER, "choose_opponent", seat)
            self.assertEqual([s["value"] for s in own.steps if s.get("property") == "power"],
                             [1, 1, 2, 3, 3, 3, 3])
            self.assertEqual([s["value"] for s in foe.steps if s.get("property") == "power"],
                             [2, 2, 2, 2, 2, 3, 4])
            self.assertEqual(sum(s["op"] == "announce_spell" for s in own.steps), 2)

    def test_color_payment_and_chains_first_draw_are_independent(self):
        for seat in (0, 1):
            paid = self.board(model.DROWNER, "foe_blue_paid", seat)
            red = self.board(model.DROWNER, "nonblue", seat)
            self.assertEqual(len(paid.library[seat]), 2)
            self.assertEqual(len(red.library[seat]), 4)
            self.assertEqual(red.remaining_mana[seat], [1, 0, 0, 0, 0, 0])
            first = self.board(model.CHAINS, "first_and_additional", seat)
            empty = self.board(model.CHAINS, "outside_empty", seat)
            self.assertEqual(sum(s["op"] == "draw_step" for s in first.steps), 1)
            self.assertEqual(first.objects[f"lib_{1-seat}_0"]["zone"], "graveyard")
            self.assertEqual(first.objects[f"lib_{1-seat}_1"]["zone"], "hand")
            self.assertEqual(empty.objects[f"lib_{1-seat}_0"]["zone"], "graveyard")
