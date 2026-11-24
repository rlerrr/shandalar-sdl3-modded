"""Exact entry card guard and fixture-boundary unit checks."""
import copy
import unittest

import entry_interaction_cards_and_selection_recipes as model


class EntryInteractionCardsAndSelectionGuardTests(unittest.TestCase):
    def setUp(self):
        self.pins = model.profiles()
        merged = self.pins["sources"] | self.pins["witnesses"]
        self.oracle = {name: pair["oracle"] for name, pair in merged.items()}
        self.rows = {name: pair["registry"] for name, pair in merged.items()}

    def test_full_pinned_shape_and_variants(self):
        model.validate_witnesses(self.oracle, self.rows)
        self.assertEqual(len(self.pins["sources"]), 16)
        for name, source in self.pins["sources"].items():
            record = source["oracle"]
            shape = model.classify(record, source["registry"]["csvid"])
            variants = model.variants(shape)
            declaration = dict(family=model.FAMILY, version=2, params=shape,
                               oracle_text=record["oracle_text"], variants=variants)
            self.assertEqual(model.spec(declaration, record)["csvid"], shape["csvid"])
            with self.assertRaises(ValueError):
                model.spec(dict(declaration, version=True), record)
            with self.assertRaises(ValueError):
                model.spec(dict(declaration, variants=variants + variants[:1]), record)
            with self.assertRaises(ValueError):
                model.spec(declaration, dict(record, oracle_text=record["oracle_text"] + "x"))

    def test_witness_mutations_fail_closed(self):
        for name in model.WITNESSES:
            bad_rows = copy.deepcopy(self.rows)
            bad_rows[name]["csvid"] += 1
            with self.assertRaises(ValueError):
                model.validate_witnesses(self.oracle, bad_rows)
            bad_oracle = dict(self.oracle)
            bad_oracle[name] = dict(self.oracle[name], oracle_id="different")
            with self.assertRaises(ValueError):
                model.validate_witnesses(bad_oracle, self.rows)

