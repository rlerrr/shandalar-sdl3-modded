"""Guard the exact entry routes and independent printed costs and outcomes."""

import unittest

import entry_trigger_cards_and_selection_recipes as model


class EntrySelectionGuard(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        groups = model.profiles()
        pins = groups["sources"] | groups["witnesses"]
        cls.oracle = {name: pin["oracle"] for name, pin in pins.items()}
        cls.rows = {name: pin["registry"] for name, pin in pins.items()}
        cls.names = {pin["registry"]["csvid"]: name for name, pin in pins.items()}

    def board(self, csvid, mode, seat):
        game = model.Board(csvid, self.names[csvid], mode, seat, self.rows, self.oracle)
        game.run()
        return game

    def test_exact_sources_and_decline_scope(self):
        model.validate_witnesses(self.oracle, self.rows)
        for name, pin in model.profiles()["sources"].items():
            csvid, record = pin["registry"]["csvid"], self.oracle[name]
            shape = model.classify(record, csvid)
            entry = dict(family=model.FAMILY, version=2, params=shape, oracle_text=record["oracle_text"],
                         variants=model.variants(shape))
            self.assertEqual(model.spec(entry, record)["csvid"], csvid)
            with self.assertRaises(ValueError):
                model.spec(entry, {**record, "oracle_text": record["oracle_text"] + " Draw one."})
        self.assertNotIn("decline_draw_1", model.variants(dict(csvid=model.LAMPLIGHTER)))

    def test_delayed_owner_draw_and_qualifying_prowl(self):
        for owner in (0, 1):
            removed = self.board(model.PYKNITE, "removed_before_upkeep", owner)
            self.assertEqual(removed.objects["source"]["zone"], "graveyard")
            self.assertEqual([s["player"] for s in removed.steps if s["op"] == "upkeep_step"], [1 - owner])
            self.assertEqual(len(removed.library[owner]), 4)
            paid = self.board(model.LATCHKEY, "paid_prowl", owner)
            normal = self.board(model.LATCHKEY, "normal_after_hit", owner)
            self.assertEqual(len(paid.library[owner]), 4)
            self.assertEqual(len(normal.library[owner]), 5)
            ops = [s["op"] for s in paid.steps]
            self.assertLess(ops.index("combat_damage"), ops.index("cast"))

    def test_lands_keep_correct_controller_and_ability_class(self):
        for owner in (0, 1):
            wood = self.board(model.WOOD, "foe_land_mana", owner)
            self.assertEqual(wood.objects["land"]["player"], 1 - owner)
            self.assertIn({"op": "can_cast", "ref": "source", "value": 1}, wood.steps)
            self.assertNotIn("cast", [step["op"] for step in wood.steps])
            own_wood = self.board(model.WOOD, "own_land_mana", owner)
            self.assertIn({"op": "choose_color", "player": owner, "value": 2}, own_wood.steps)
            self.assertIn({"op": "activate_granted_mana", "ref": "land", "other": "source", "value": 1},
                          own_wood.steps)
            sun = self.board(model.SUN, "restore_search", owner)
            self.assertEqual([s["value"] for s in sun.steps if s.get("ref") == "search_0"], [1, 0, 1])

    def test_consumed_artifacts_and_optional_loot(self):
        for owner in (0, 1):
            charged = self.board(model.KALEIDO, "five_colors", owner)
            self.assertEqual(charged.objects["source"]["zone"], "graveyard")
            vial = self.board(model.VIAL, "deny_block", owner)
            self.assertEqual(vial.objects["source"]["zone"], "graveyard")
            self.assertEqual([s["value"] for s in vial.steps if s["op"] == "block"], [0])
            loots = self.board(model.LAMPLIGHTER, "draw_discard", owner)
            self.assertEqual(loots.objects[f"library_{owner}_0"]["zone"], "graveyard")
