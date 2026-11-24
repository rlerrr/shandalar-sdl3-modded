"""Guard exact pinned records and distinct regeneration and end-phase receipts."""

import unittest

import regeneration_follow_on_conditional_costs_recipes as model


class ConditionalCostsGuard(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        profiles = model.profiles()
        pins = profiles["sources"] | profiles["witnesses"]
        cls.oracle = {name: pin["oracle"] for name, pin in pins.items()}
        cls.rows = {name: pin["registry"] for name, pin in (pins | profiles["token_witnesses"]).items()}
        cls.names = {pin["registry"]["csvid"]: name for name, pin in pins.items()}

    def board(self, csvid, mode, seat):
        board = model.Board(csvid, self.names[csvid], mode, seat, self.rows, self.oracle)
        board.run()
        return board

    def test_exact_oracle_and_mirrors(self):
        model.validate_witnesses(self.oracle, self.rows)
        for name, pin in model.profiles()["sources"].items():
            csvid = pin["registry"]["csvid"]
            record = self.oracle[name]
            shape = model.classify(record, csvid)
            decl = dict(family=model.FAMILY, version=2, params=shape, oracle_text=record["oracle_text"],
                        variants=model.variants(shape))
            self.assertEqual(model.spec(decl, record)["csvid"], csvid)
            self.assertEqual(len(decl["variants"]), 2 * len(model.MODES[csvid]))
            with self.assertRaises(ValueError):
                model.spec(decl, {**record, "oracle_text": record["oracle_text"] + " Draw a card."})

    def test_starfish_receipt_delays_tokens_until_actual_end(self):
        token = model.STARFISH_TOKEN["registry"]["csvid"]
        for owner in (0, 1):
            twice = self.board(model.SPINY, "twice", owner)
            self.assertFalse(any(card["name"] == "Starfish" for card in twice.setup))
            steps = twice.steps
            end = next(i for i, step in enumerate(steps) if step["op"] == "advance_turn")
            self.assertEqual(sum(step["op"] == "destroy" for step in steps[:end]), 2)
            counts = [step["value"] for step in steps if step.get("index") == token
                      and step["op"] == "assert_zone_multiplicity" and step.get("player", 0) == owner]
            self.assertEqual(counts, [0, 0, 0, 2])
            self.assertEqual({step["property"]: step["value"] for step in steps if step["op"] == "assert_matching_permanents"},
                             dict(power=0, toughness=1, color=4, token=1))

    def test_changeling_static_bonus_and_independent_payment(self):
        for owner in (0, 1):
            types = self.board(model.CHANGELING, "shared_types", owner)
            self.assertEqual([s["value"] for s in types.steps if s["op"] == "assert_card"
                              and s.get("property") == "power"], [3, 3, 4])
            paid = self.board(model.CHANGELING, "pay_regenerate", owner)
            short = self.board(model.CHANGELING, "short_generic", owner)
            self.assertEqual(paid.objects["source"]["zone"], "battlefield")
            self.assertEqual(short.objects["source"]["zone"], "graveyard")

    def test_yule_counts_chosen_mana_value_even_if_destroy_fails(self):
        for owner in (0, 1):
            success = self.board(model.YULE, "allied_creature", owner)
            immune = self.board(model.YULE, "indestructible", owner)
            self.assertEqual(success.objects["other"]["zone"], "graveyard")
            self.assertEqual(immune.objects["other"]["zone"], "battlefield")
            self.assertEqual([s["value"] for s in immune.steps if s.get("property") == "plus1_counters"], [0, 3])
            self.assertEqual([s["player"] for s in immune.steps if s["op"] == "upkeep_step"], [1 - owner, owner])
