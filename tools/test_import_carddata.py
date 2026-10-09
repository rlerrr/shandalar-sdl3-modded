"""Guard explicit binding recovery against source-data drift."""
import json
import tempfile
import unittest
from pathlib import Path
from import_carddata import load_handler_overrides, override_handler


class HandlerOverrideTest(unittest.TestCase):
    def setUp(self):
        self.record = {"name": "Aegis Automaton", "source_address": "0x00401000",
                       "handler": "manalinkeh_card_aegis_automaton", "reason": "Existing compiled implementation"}

    def test_exact_dummy_record_is_recovered(self):
        self.assertEqual(override_handler({16644: self.record}, 16644, "Aegis Automaton", 0x401000, "card_dummy"),
                         "manalinkeh_card_aegis_automaton")

    def test_other_dummy_records_are_preserved(self):
        self.assertEqual(override_handler({16644: self.record}, 98, "Giant Spider", 0x401000, "card_dummy"), "card_dummy")

    def test_changed_identity_or_binding_requires_review(self):
        for name, address, handler in (("Different card", 0x401000, "card_dummy"),
                                       ("Aegis Automaton", 0x2000001, "manalinkeh_card_other"),
                                       ("Aegis Automaton", 0x401000, "card_other")):
            with self.subTest(name=name, address=address, handler=handler), self.assertRaisesRegex(ValueError, "Stale handler override"):
                override_handler({16644: self.record}, 16644, name, address, handler)

    def test_overrides_are_explicit_dummy_replacements(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "overrides.json"
            for address in ("0x00401000", "0x02000001"):
                record = dict(self.record, source_address=address)
                path.write_text(json.dumps({"version": 1, "cards": {"16644": record}}))
                if address == "0x00401000":
                    self.assertEqual(load_handler_overrides(path), {16644: record})
                else:
                    with self.assertRaisesRegex(ValueError, "dummy address"):
                        load_handler_overrides(path)

    def test_cost_hook_flags_are_restricted(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "overrides.json"
            for flags in (0, 1, 2, -1):
                record = dict(self.record, new_field_or=flags)
                path.write_text(json.dumps({"version": 1, "cards": {"16644": record}}))
                if flags in (0, 1):
                    self.assertEqual(load_handler_overrides(path)[16644]["new_field_or"], flags)
                else:
                    with self.assertRaisesRegex(ValueError, "cost-hook flags"):
                        load_handler_overrides(path)

    def test_manifest_matches_existing_compiled_symbols(self):
        root = Path(__file__).resolve().parents[1]
        overrides = load_handler_overrides(root / "tools/carddata_handler_overrides.json")
        self.assertEqual(len(overrides), 46)
        names = set((root / "src/manalinkeh/internal_symbols.txt").read_text().split())
        asm = (root / "src/manalinkeh/ManalinkEh.asm").read_text()
        for record in overrides.values():
            symbol = record["handler"].removeprefix("manalinkeh_")
            self.assertTrue(symbol in names or "_" + symbol in asm, symbol)


if __name__ == "__main__":
    unittest.main()
