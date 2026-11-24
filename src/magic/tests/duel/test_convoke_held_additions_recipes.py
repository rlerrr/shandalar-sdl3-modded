"""Pin the complete rules and independently check the held Convoke controls."""

import unittest

import convoke_held_additions_recipes as model


class HeldConvokeGuard(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        pins = model.profiles()["sources"] | model.profiles()["witnesses"]
        cls.oracle = {name: pin["oracle"] for name, pin in pins.items()}
        cls.rows = {name: pin["registry"] for name, pin in pins.items()}

    def game(self, name, mode, seat):
        csvid = self.rows[name]["csvid"]
        board = model.Board(name, mode, seat, self.rows, self.oracle)
        if csvid == model.SERAPH:
            action = board.seraph if mode in model.RAW_MODES[csvid] else board.seraph_addition
        else:
            action = board.chief if mode in model.RAW_MODES[csvid] else board.chief_addition
        action()
        return board

    def test_whole_record_and_version(self):
        model.validate_witnesses(self.oracle, self.rows)
        for name, pin in model.profiles()["sources"].items():
            record = self.oracle[name]
            shape = model.classify(record, pin["registry"]["csvid"])
            declaration = dict(family=model.FAMILY, version=2, params=shape,
                               oracle_text=record["oracle_text"], variants=model.variants(shape))
            self.assertEqual(model.spec(declaration, record)["csvid"], shape["csvid"])
            with self.assertRaises(ValueError):
                model.spec(declaration, {**record, "oracle_text": record["oracle_text"] + "\nDraw a card."})
            with self.assertRaises(ValueError):
                model.spec({**declaration, "version": True}, record)
            with self.assertRaises(ValueError):
                model.spec({**declaration, "variants": ["invalid_0"]}, record)

    def test_seraph_counts_controlled_creatures_on_actual_entry(self):
        for seat in (0, 1):
            board = self.game("Seraph of the Masses", "paid_entry", seat)
            self.assertEqual([s["ref"] for s in board.steps if s["op"] == "cast"], ["source", "new_creature"])
            self.assertEqual([s["value"] for s in board.steps if s.get("property") == "power"], [1, 2])
            self.assertFalse(any(s["op"] == "choose_target" for s in board.steps))

    def test_seraph_counter_preserves_convoke_payments_and_uses_real_response(self):
        for seat in (0, 1):
            board = self.game("Seraph of the Masses", "countered", seat)
            ordered = [s["op"] for s in board.steps if s["op"] in
                       {"cast", "announce_spell", "cast_response", "resolve_response"}]
            self.assertEqual(ordered, ["cast", "announce_spell", "cast_response", "resolve_response"])
            self.assertEqual([board.objects[r]["zone"] for r in ("source", "counterspell")], ["graveyard"] * 2)
            self.assertEqual([s["value"] for s in board.steps if s.get("property") == "tapped"
                              and s.get("ref") == "white_helper"], [1, 1])
            scripted = [s["ref"] for s in board.steps if s["op"] == "choose_target"]
            self.assertEqual(scripted, ["white_helper", "generic_helper"] if seat == 0 else [])

    def test_chief_removal_loses_grant_and_normal_payment_preserves_creatures(self):
        for seat in (0, 1):
            lost = self.game("Chief Engineer", "grant_lost", seat)
            self.assertEqual([(s["ref"], s["value"]) for s in lost.steps if s["op"] == "cast"],
                             [("source", 1), ("removal", 1), ("artifact_spell", 0), ("artifact_spell", 1)])
            self.assertEqual(lost.objects["source"]["zone"], "graveyard")
            paid = self.game("Chief Engineer", "normal_payment", seat)
            self.assertEqual([(s["ref"], s["value"]) for s in paid.steps if s["op"] == "cast"],
                             [("source", 1), ("artifact_spell", 1)])
            self.assertEqual({s["ref"] for s in paid.steps if s.get("property") == "tapped" and not s["value"]},
                             {"source", "other_payer"})
