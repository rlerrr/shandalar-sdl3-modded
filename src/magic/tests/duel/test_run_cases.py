"""Keep missing implementations separate without relaxing the regression gate."""
import json
import tempfile
import unittest
from pathlib import Path
from run_cases import classify_outcome, load_unimplemented, validate_expectations


class RunnerExpectationsTest(unittest.TestCase):
    def setUp(self):
        self.name = "Behavior.Missing"
        self.case = dict(filter=self.name, card="Missing spell", csvid=100, oracle_id="oracle", handler="card_dummy")
        self.record = dict(card="Missing spell", csvid=100, oracle_id="oracle", handler="card_dummy",
                           kind="assertion", assertion_count=1, reason="Reviewed missing handler", patterns=["legal cast rejected"])
        self.missing = {self.name: self.record}
        self.manifest = dict(cases=[self.case])

    def test_expected_missing_assertion_is_not_a_known_bug_or_pass(self):
        validate_expectations(self.manifest, {}, self.missing)
        self.assertEqual(classify_outcome(self.name, "assertion", 1, "legal cast rejected", {}, self.missing), "unimplemented")
        self.assertEqual(classify_outcome(self.name, "assertion", 1, "legal cast rejected", self.missing), "known_failure")

    def test_missing_case_still_rejects_changed_failures_and_unexpected_success(self):
        for kind, count, text, status in (
                ("assertion", 2, "legal cast rejected", "failed"),
                ("assertion", 1, "different error", "failed"),
                ("harness", 1, "legal cast rejected", "failed"),
                ("crash", 0, "legal cast rejected", "failed"),
                ("passed", 0, "", "unexpected_pass")):
            with self.subTest(kind=kind, count=count, text=text):
                self.assertEqual(classify_outcome(self.name, kind, count, text, {}, self.missing), status)

    def test_dummy_alone_never_marks_a_vanilla_case_as_missing(self):
        self.assertEqual(classify_outcome("Behavior.Vanilla", "passed", 0, "", {}, self.missing), "passed")
        self.assertEqual(classify_outcome("Behavior.Vanilla", "assertion", 1, "legal cast rejected", {}, self.missing), "failed")

    def test_an_implemented_handler_or_changed_identity_requires_review(self):
        for key, value in (("handler", "manalinkeh_card_new"), ("card", "Other card"), ("csvid", 101), ("oracle_id", "other")):
            with self.subTest(key=key), self.assertRaisesRegex(ValueError, "Stale unimplemented"):
                validate_expectations(dict(cases=[dict(self.case, **{key: value})]), {}, self.missing)

    def test_overlap_stale_cases_and_invalid_missing_kinds_are_rejected(self):
        with self.assertRaisesRegex(ValueError, "both known failures"):
            validate_expectations(self.manifest, self.missing, self.missing)
        with self.assertRaisesRegex(ValueError, "Stale unimplemented"):
            validate_expectations(dict(cases=[]), {}, self.missing)
        for key, value in (("handler", "other"), ("kind", "harness"), ("kind", "crash")):
            with self.subTest(key=key), self.assertRaisesRegex(ValueError, "must document"):
                validate_expectations(self.manifest, {}, {self.name: dict(self.record, **{key: value})})

    def test_incomplete_missing_signatures_cannot_hide_assertions(self):
        for key, value in (("reason", ""), ("patterns", []), ("assertion_count", 0)):
            with self.subTest(key=key), self.assertRaisesRegex(ValueError, "needs a reason"):
                classify_outcome(self.name, "assertion", 1, "legal cast rejected", {}, {self.name: dict(self.record, **{key: value})})

    def test_companion_default_and_explicit_file_loading(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            baseline = root / "known_failures.json"
            self.assertEqual(load_unimplemented(baseline), {})
            path = root / "unimplemented.json"
            path.write_text(json.dumps(dict(version=1, cases=self.missing)))
            self.assertEqual(load_unimplemented(baseline), self.missing)
            other = root / "custom.json"
            other.write_text(json.dumps(dict(version=1, cases={})))
            self.assertEqual(load_unimplemented(baseline, other), {})
            other.write_text(json.dumps(dict(version=2, cases={})))
            with self.assertRaisesRegex(ValueError, "Unsupported unimplemented schema"):
                load_unimplemented(baseline, other)


if __name__ == "__main__":
    unittest.main()
