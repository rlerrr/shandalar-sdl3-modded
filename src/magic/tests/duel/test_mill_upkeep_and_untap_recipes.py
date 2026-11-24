"""Guard exact mill variants and the newly added effect distinctions."""

import unittest

import mill_upkeep_and_untap_recipes as model


class MillUpkeepAndUntapGuard(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        pins = model.profiles()["sources"] | model.profiles()["witnesses"]
        cls.oracle = {name: pin["oracle"] for name, pin in pins.items()}
        cls.rows = {name: pin["registry"] for name, pin in pins.items()}
        cls.by_id = {value["csvid"]: name for name, value in cls.rows.items()}

    def build_board(self, csvid, mode, seat):
        board = model.Board(csvid, self.by_id[csvid], mode, seat, self.rows, self.oracle)
        board.run()
        return board

    def test_exact_record_and_version(self):
        model.validate_witnesses(self.oracle, self.rows)
        for name, pin in model.profiles()["sources"].items():
            rules = self.oracle[name]
            shape = model.classify(rules, pin["registry"]["csvid"])
            decl = dict(family=model.FAMILY, version=2, params=shape,
                        oracle_text=rules["oracle_text"], variants=model.variants(shape))
            self.assertEqual(model.spec(decl, rules)["csvid"], shape["csvid"])
            with self.assertRaises(ValueError):
                model.spec(decl, {**rules, "oracle_text": rules["oracle_text"] + "\nDraw a card."})
            with self.assertRaises(ValueError):
                model.spec({**decl, "version": True}, rules)

    def test_deep_spawn_choice_and_real_response_skip(self):
        for seat in (0, 1):
            declined = self.build_board(model.DEEP, "tax_declined", seat)
            self.assertEqual(declined.objects["source"]["zone"], "graveyard")
            self.assertEqual(len(declined.library[seat]), 6)
            full = self.build_board(model.DEEP, "shroud_trample", seat)
            self.assertEqual(full.objects["removal"]["zone"], "graveyard")
            self.assertEqual(len(full.library[seat]), 2)
            self.assertEqual(full.life[1 - seat], 16)
            self.assertEqual([step["value"] for step in full.steps if step.get("property") == "tapped"],
                             [1, 1, 1, 0])
            operations = [step["op"] for step in full.steps]
            self.assertLess(operations.index("announce_spell"), operations.index("activate_response"))
            self.assertLess(operations.index("activate_response"), operations.index("resolve_response"))

    def test_splinterfright_counts_only_own_creature_cards(self):
        for seat in (0, 1):
            combat = self.build_board(model.SPLINTER, "growth_trample", seat)
            mixed = self.build_board(model.SPLINTER, "mixed_prefix", seat)
            self.assertEqual([step["value"] for step in combat.steps if step.get("property") == "power"], [2, 2, 4])
            self.assertEqual([step["value"] for step in mixed.steps if step.get("property") == "power"], [1, 1, 1, 2])
            self.assertEqual(combat.life[1 - seat], 18)

    def test_serpent_draw_and_patient_land_linked_draw_are_distinct(self):
        for seat in (0, 1):
            long = self.build_board(model.SERPENT, "seven_and_draw", seat)
            short = self.build_board(model.SERPENT, "short_library", seat)
            drawn = self.build_board(model.PATIENT, "mixed_draw", seat)
            nonland = self.build_board(model.PATIENT, "short_no_lands", seat)
            self.assertEqual((len(long.library[seat]), len(short.library[seat])), (1, 0))
            self.assertEqual((len(drawn.library[seat]), len(nonland.library[seat])), (2, 5))
            self.assertEqual((len(drawn.library[1 - seat]), len(nonland.library[1 - seat])), (2, 0))
            self.assertIn(dict(op="choose_player", player=seat, value=1 - seat), drawn.steps)
