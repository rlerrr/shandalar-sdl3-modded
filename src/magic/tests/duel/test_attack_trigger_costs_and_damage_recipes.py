"""Guard pinned attack cards, seat ownership and distinct printed costs."""

import unittest

import attack_trigger_costs_and_damage_recipes as model


class AttackCostsGuard(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        profiles = model.profiles()
        pins = profiles["sources"] | profiles["witnesses"]
        cls.oracle = {name: pin["oracle"] for name, pin in pins.items()}
        cls.rows = {name: pin["registry"] for name, pin in pins.items()}
        cls.names = {pin["registry"]["csvid"]: name for name, pin in pins.items()}

    def board(self, csvid, mode, seat):
        board = model.Board(csvid, self.names[csvid], mode, seat, self.rows, self.oracle)
        board.run()
        return board

    def test_pins_and_actor_scope(self):
        model.validate_witnesses(self.oracle, self.rows)
        for name, pin in model.profiles()["sources"].items():
            csvid = pin["registry"]["csvid"]
            record = self.oracle[name]
            shape = model.classify(record, csvid)
            declaration = dict(family=model.FAMILY, version=2, params=shape, oracle_text=record["oracle_text"],
                               variants=model.variants(shape))
            self.assertEqual(model.spec(declaration, record)["csvid"], csvid)
            with self.assertRaises(ValueError):
                model.spec(declaration, {**record, "oracle_text": record["oracle_text"] + " Draw a card."})
        self.assertIn("decline_exile_0", model.variants(dict(csvid=model.ROTTING)))
        self.assertNotIn("paid_then_block_0", model.variants(dict(csvid=model.MARAUDER)))

    def test_sacrifice_and_exile_sources(self):
        for owner in (0, 1):
            exile = self.board(model.ROTTING, "exile_block", owner)
            self.assertEqual(exile.objects["grave_fee"]["zone"], "exile")
            self.assertEqual(exile.objects["enemy_grave"]["zone"], "graveyard")
            empty = self.board(model.ROTTING, "empty_block", owner)
            self.assertEqual(empty.life, [20, 20])
            self.assertEqual(empty.objects["source"]["zone"], "graveyard")
            paid = self.board(model.THING, "island", owner)
            wrong = self.board(model.THING, "wrong_land", owner)
            self.assertEqual(paid.objects["fee"]["zone"], "graveyard")
            self.assertEqual(wrong.objects["rival_island"]["zone"], "battlefield")
            self.assertEqual(wrong.objects["source"]["zone"], "graveyard")

    def test_dash_and_defender_decisions(self):
        for owner in (0, 1):
            dashed = self.board(model.DASH, "dash_attack", owner)
            self.assertLess([s["op"] for s in dashed.steps].index("attack"),
                            [s["op"] for s in dashed.steps].index("advance_turn"))
            self.assertEqual(dashed.objects["source"]["zone"], "hand")
            drawn = self.board(model.HARBOR, "draw_accept", owner)
            self.assertEqual(drawn.objects["top"]["zone"], "hand")
            self.assertEqual(drawn.objects["top"]["player"], 1 - owner)
            declined = self.board(model.SIBILANT, "draw_decline", owner)
            self.assertEqual(declined.objects["top"]["zone"], "library")

    def test_paid_damage_boundaries(self):
        for owner in (0, 1):
            fire = self.board(model.FLAMEBLAST, "x_planeswalker", owner)
            self.assertEqual([s["value"] for s in fire.steps if s["op"] == "choose_x"], [2])
            self.assertEqual([s["value"] for s in fire.steps if s["op"] == "assert_counter"], [1])
            wolf = self.board(model.WOLF, "discard_zero", owner)
            self.assertEqual(wolf.objects["discard"]["zone"], "graveyard")
            self.assertEqual(wolf.objects["damage_target"]["zone"], "battlefield")
            lion = self.board(model.MTENDA, "blocked_payment", owner)
            self.assertEqual(lion.objects["source"]["zone"], "graveyard")
            self.assertEqual(lion.objects["blocker"]["zone"], "battlefield")

    def test_ventmaw_protected_and_ordinary_green_split_at_real_boundary(self):
        for owner in (0, 1):
            game = self.board(model.VENTMAW, "real_phase_drain", owner)
            steps = game.steps
            boundary = next(i for i, s in enumerate(steps) if s["op"] == "end_combat_step")
            self.assertEqual(steps[boundary], {"op": "end_combat_step", "value": 0})
            self.assertNotIn("end_combat", [step["op"] for step in steps])
            land = next(i for i,s in enumerate(steps) if s["op"] == "activate"
                        and s.get("ref") == "ordinary_mana_source")
            damage = next(i for i,s in enumerate(steps) if s["op"] == "combat_damage")
            spend = next(i for i,s in enumerate(steps) if s["op"] == "cast"
                         and s.get("ref") == "follow_spell")
            self.assertLess(damage,land)
            self.assertLess(land,boundary)
            self.assertLess(boundary,spend)
            for color, pair in ((3,(4,3)),(4,(3,3))):
                for stage, value in zip((steps[land:boundary],steps[boundary:spend]),pair):
                    self.assertIn({"op":"assert_mana","player":owner,"color":color,"value":value},stage)
            self.assertEqual(game.objects["follow_spell"]["zone"],"battlefield")
