"""Guard whole-record declarations and Jin-Gitaxias's actual turn boundaries."""

import unittest

import flash_remaining_rule_effects_recipes as model


class FlashRuleEffectsGuard(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        pins = model.profiles()["sources"] | model.profiles()["witnesses"]
        cls.oracle = {name: pin["oracle"] for name, pin in pins.items()}
        cls.rows = {name: pin["registry"] for name, pin in pins.items()}

    def jin(self, mode, seat):
        board = model.Board(model.JIN, "Jin-Gitaxias, Core Augur", mode, seat, self.rows, self.oracle)
        board.run()
        return board

    def test_whole_record_and_recipe_version(self):
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

    def test_end_step_draw_and_source_removed_control(self):
        for seat in (0, 1):
            draw = self.jin("own_end", seat)
            removed = self.jin("source_removed", seat)
            actions = [step["op"] for step in draw.steps]
            self.assertLess(actions.index("cast_response"), actions.index("advance_turn"))
            self.assertLess(actions.index("advance_turn"), actions.index("cleanup"))
            self.assertEqual(len(draw.library[seat]), 1)
            self.assertEqual(len(removed.library[seat]), 8)
            self.assertEqual(removed.objects["source"]["zone"], "graveyard")

    def test_owner_with_spellbook_draws_seven_over_existing_two(self):
        for seat in (0, 1):
            board = self.jin("unlimited_hand", seat)
            self.assertEqual(board.objects["no_maximum"]["name"], model.SPELLBOOK)
            self.assertEqual(sum(card["player"] == seat and card["zone"] == "hand"
                                 for card in board.objects.values()), 9)

    def test_only_real_discard_phase_can_measure_hand_limit(self):
        for name, maximum in (("hand_limit", 0), ("normal_limit_after_removal", 7)):
            board = self.jin(name, 0)
            position = next(i for i, step in enumerate(board.steps) if step["op"] == "ai_turns")
            self.assertEqual(board.steps[position]["value"], 2)
            self.assertIn(dict(op="assert_zone", player=1, zone="hand", value=maximum),
                          board.steps[position + 1:])
        shape = dict(csvid=model.JIN)
        self.assertFalse(any(mode.endswith("_1") and "limit" in mode and "unlimited" not in mode
                             for mode in model.variants(shape)))
