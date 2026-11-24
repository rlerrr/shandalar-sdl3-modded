"""Guard whole Annihilator and Myriad primaries and cast and combat receipts."""

import unittest

import attack_trigger_follow_on_annihilator_and_myriad_recipes as model


class AnnihilatorMyriadGuard(unittest.TestCase):
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

    def test_exact_pins_and_mirrors(self):
        model.validate_witnesses(self.oracle, self.rows)
        for name, pin in model.profiles()["sources"].items():
            csvid, record = pin["registry"]["csvid"], self.oracle[name]
            shape = model.classify(record, csvid)
            decl = dict(family=model.FAMILY, version=2, params=shape, oracle_text=record["oracle_text"],
                        variants=model.variants(shape))
            self.assertEqual(model.spec(decl, record)["csvid"], csvid)
            with self.assertRaises(ValueError):
                model.spec(decl, {**record, "oracle_text": record["oracle_text"] + " Draw a card."})
        self.assertNotIn("decline_draw_1", model.variants(dict(csvid=model.VIPER)))

    def test_kozilek_cast_draw_and_shuffle_use_disjoint_library_and_hand(self):
        for owner in (0, 1):
            draw = self.board(model.KOZILEK, "draw_cast_attack", owner)
            self.assertEqual(len(draw.library[owner]), 1)
            self.assertEqual([draw.objects[ref]["name"] for ref in draw.library[owner]], [model.MOUNTAIN])
            shuffled = self.board(model.KOZILEK, "murder_shuffle", owner)
            self.assertEqual(len(shuffled.library[owner]), 4)
            self.assertEqual(sum(s["op"] == "choose_dialog" for s in shuffled.steps), int(owner == 0))

    def test_pathrazer_three_real_assignments_and_opponent_sacrifice(self):
        for owner in (0, 1):
            three = self.board(model.PATHRAZER, "three_blockers", owner)
            self.assertEqual([s["value"] for s in three.steps if s["op"] == "block"], [1, 1, 1])
            one = self.board(model.PATHRAZER, "one_blocker", owner)
            self.assertEqual([s["value"] for s in one.steps if s["op"] == "block"], [0])
            take = self.board(model.BETRAYS, "annihilator_take", owner)
            self.assertEqual([take.objects[f"defending_{i}"]["player"] for i in range(2)], [owner, owner])
            destroy = self.board(model.BETRAYS, "destroy_not_sacrifice", owner)
            self.assertEqual(destroy.objects["enemy_creature"]["player"], 1 - owner)
            self.assertEqual(destroy.objects["enemy_creature"]["zone"], "graveyard")

    def test_myriad_uses_no_copy_and_distinct_printed_riders(self):
        for owner in (0, 1):
            viper = self.board(model.VIPER, "draw_hit", owner)
            self.assertEqual(len(viper.library[owner]), 1)
            herald = self.board(model.HERALD, "flying_vigilance", owner)
            self.assertTrue(any(s.get("property") == "tapped" and s["value"] == 0 for s in herald.steps))
            leader = self.board(model.WARCHIEF, "fresh_haste", owner)
            self.assertFalse(any(s["op"] == "advance_turn" for s in leader.steps))
            for game in (viper, herald, leader):
                self.assertEqual(sum(obj["name"] == game.name for obj in game.objects.values()), 1)
