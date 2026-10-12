"""Regressions for discovery, identity, and independent case authoring."""
import json
import tempfile
import unittest
from pathlib import Path

from case_files import load_cases


class CaseFilesTest(unittest.TestCase):
    def setUp(self):
        # CTest runs in its build directory; direct runs also stay inside the workspace.
        self.temp = tempfile.TemporaryDirectory(dir=Path.cwd())
        self.assertEqual(Path(self.temp.name).resolve().parent, Path.cwd().resolve())
        self.addCleanup(self.temp.cleanup)
        self.directory = Path(self.temp.name) / "cases"
        self.registry = {"Infantry Veteran": {"csvid": 2723}, "Furnace Whelp": {"csvid": 7059}}

    def write(self, relative, document):
        path = self.directory / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps(document), encoding="utf-8")

    def card_file(self, name="Infantry Veteran", csvid=2723, case="Veteran_Taps"):
        return {"version": 1, "card": name, "csvid": csvid,
                "cases": [{"name": case, "fixture": "combat", "steps": []}]}

    def test_card_identity_is_inherited_and_source_is_relative(self):
        self.write("cards/bucket/veteran.json", self.card_file())
        case, = load_cases(self.directory, self.registry)
        self.assertEqual(case["card"], "Infantry Veteran")
        self.assertEqual(case["_source_file"], "cases/cards/bucket/veteran.json")

    def test_nested_discovery_and_order_do_not_depend_on_card_homes(self):
        self.write("cards/z/veteran.json", self.card_file(case="A_Veteran"))
        self.write("cards/a/whelp.json", self.card_file("Furnace Whelp", 7059, "Z_Whelp"))
        original = load_cases(self.directory, self.registry)
        moved = self.directory / "cards/another-set/veteran.json"
        moved.parent.mkdir(parents=True)
        (self.directory / "cards/z/veteran.json").rename(moved)
        self.assertEqual([c["name"] for c in original], [c["name"] for c in load_cases(self.directory, self.registry)])

    def test_scenarios_can_reference_multiple_cards(self):
        self.write("scenarios/snapshots.json", {"version": 1, "cases": [
            {"name": "Snapshot_Veteran", "card": "Infantry Veteran"},
            {"name": "Snapshot_Whelp", "card": "Furnace Whelp"}]})
        self.assertEqual(len(load_cases(self.directory, self.registry)), 2)

    def test_duplicate_case_reports_both_sources(self):
        self.write("cards/veteran.json", self.card_file())
        self.write("scenarios/duplicate.json", {"version": 1, "cases": [
            {"name": "Veteran_Taps", "card": "Infantry Veteran"}]})
        with self.assertRaisesRegex(ValueError, r"duplicate case.*cards/veteran.json"):
            load_cases(self.directory, self.registry)

    def test_card_has_one_home_even_in_different_sets(self):
        self.write("cards/set-a/veteran.json", self.card_file(case="Veteran_One"))
        self.write("cards/set-b/veteran.json", self.card_file(case="Veteran_Two"))
        with self.assertRaisesRegex(ValueError, "duplicate card home"):
            load_cases(self.directory, self.registry)

    def test_mismatched_id_or_case_card_is_rejected(self):
        for document, message in [(self.card_file(csvid=7059), "name and CSVID"),
                                  (dict(self.card_file(), cases=[{"name": "Wrong", "card": "Furnace Whelp"}]), "conflicts")]:
            with self.subTest(message=message):
                self.write("cards/veteran.json", document)
                with self.assertRaisesRegex(ValueError, message):
                    load_cases(self.directory, self.registry)

    def test_missing_cases_and_unsupported_versions_are_not_silent(self):
        with self.assertRaisesRegex(ValueError, "No behavior case files"):
            load_cases(self.directory, self.registry)
        for document, message in [({"version": 2, "cases": [{}]}, "schema"),
                                  ({"version": 1, "cases": []}, "nonempty")]:
            with self.subTest(message=message):
                self.write("broken.json", document)
                with self.assertRaisesRegex(ValueError, message):
                    load_cases(self.directory, self.registry)

    def test_alternate_records_require_authored_rules_scenarios(self):
        self.registry["Infantry Veteran"]["kind"] = "alternate"
        doc = self.card_file()
        self.write("cards/alternate.json", doc)
        self.assertEqual(len(load_cases(self.directory, self.registry)), 1)
        self.write("cards/alternate.json", dict(doc, recipes=[{"family": "creature_keywords"}]))
        with self.assertRaisesRegex(ValueError, "require a primary card"):
            load_cases(self.directory, self.registry, {"Infantry Veteran": {}})

    def test_reviewed_registry_classification_applied(self):
        from generate_cases import registry
        rows = {r["csvid"]: r for r in registry()}
        self.assertEqual(rows[910]["kind"], "alternate")
        self.assertIn("Mishra's Factory", rows[910]["kind_reason"])
        self.assertEqual(rows[2723]["kind"], "card")

    def test_registry_classification_rejects_stale_or_unreviewed_entries(self):
        from unittest.mock import patch
        import generate_cases
        import import_duel_oracle
        base = {"name": "Assembly-Worker", "kind": "alternate", "reason": "Reviewed identity"}
        for doc in ([None], {"version": 2, "overrides": {}}, {"version": 1, "overrides": []},
                    {"version": 1, "overrides": {"999999": base}},
                    *({"version": 1, "overrides": {"910": dict(base, **change)}}
                      for change in ({"name": "Wrong"}, {"kind": "card"}, {"reason": ""}))):
            with self.subTest(doc=doc), patch.object(import_duel_oracle, "DATA", Path(self.temp.name)):
                (Path(self.temp.name) / "registry_kinds.json").write_text(json.dumps(doc), encoding="utf8")
                with self.assertRaises(ValueError):
                    generate_cases.registry()


if __name__ == "__main__":
    unittest.main()
