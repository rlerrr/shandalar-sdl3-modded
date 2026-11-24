"""Check whole-card pins and the real conditions for paid regeneration."""

import unittest

import regeneration_conditional_recipes as model


class ConditionalRegenerationGuard(unittest.TestCase):
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

    def test_whole_record_identity_and_mirrored_scope(self):
        model.validate_witnesses(self.oracle, self.rows)
        for name, pin in model.profiles()["sources"].items():
            csvid = pin["registry"]["csvid"]
            record = self.oracle[name]
            shape = model.classify(record, csvid)
            decl = dict(family=model.FAMILY, version=2, params=shape,
                        oracle_text=record["oracle_text"], variants=model.variants(shape))
            self.assertEqual(model.spec(decl, record)["csvid"], csvid)
            self.assertEqual(len(decl["variants"]), len(model.MODES[csvid]) * 2)
            with self.assertRaises(ValueError):
                model.spec(decl, {**record, "oracle_text": record["oracle_text"] + " Draw a card."})

    def test_formidable_counts_tapped_own_power_and_excludes_enemy_power(self):
        for owner in (0, 1):
            for mode, survives in (("exact_eight", True), ("tapped_power", True),
                                   ("under_eight", False), ("wrong_mana", False)):
                game = self.board(model.DRAGON_BEAR, mode, owner)
                checks = [step["value"] for step in game.steps if step["op"] == "assert_unused_choices"]
                self.assertEqual(checks, [int(not survives)])
                self.assertEqual(game.objects["support"]["player"], owner)
                self.assertEqual(game.objects["enemy_power"]["player"], 1 - owner)

    def test_sea_troll_history_uses_actual_declaration_and_cleanup(self):
        for owner in (0, 1):
            for mode, survives in (("history_this_turn", True), ("history_expired", False)):
                game = self.board(model.SEA, mode, owner)
                ops = [step["op"] for step in game.steps]
                self.assertLess(ops.index("block"), ops.index("advance_combat"))
                self.assertLess(ops.index("end_combat"), ops.index("choose_regeneration"))
                self.assertEqual(game.objects["source"]["zone"], "battlefield" if survives else "graveyard")
                if not survives:
                    self.assertLess(ops.index("end_combat"), ops.index("cleanup"))
                    self.assertLess(ops.index("cleanup"), ops.index("choose_regeneration"))

    def test_hellbent_and_life_cost_receipts(self):
        for owner in (0, 1):
            rats = self.board(model.RATS, "spent_last_card", owner)
            power = [step["value"] for step in rats.steps
                     if step.get("ref") == "source" and step.get("property") == "power"]
            self.assertEqual(power, [2, 3, 3])
            self.assertEqual(rats.objects["last_card"]["zone"], "battlefield")
            self.assertEqual(self.board(model.BATS, "five_life", owner).life[owner], 1)
            self.assertEqual(self.board(model.BATS, "life_short", owner).life[owner], 3)
