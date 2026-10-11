"""Keep missing implementations separate without relaxing the regression gate."""
import json
import tempfile
import unittest
from pathlib import Path
from run_cases import classify_outcome, load_unimplemented, validate_expectations, expectations_for_platform, process_outcome
import xml.etree.ElementTree as ET
import re


class RunnerExpectationsTest(unittest.TestCase):
    def test_known_crash_cannot_hide_a_preceding_rules_failure(self):
        with tempfile.TemporaryDirectory() as temporary:
            xml=Path(temporary)/'result.xml';root=ET.Element('testsuites',tests='1');case=ET.SubElement(root,'testcase')
            ET.SubElement(case,'failure',message='Expected equality of these values: power')
            ET.SubElement(case,'failure',message='unknown file\nSEH exception with code 0xc0000005 thrown in the test body.')
            ET.ElementTree(root).write(xml)
            self.assertEqual(process_outcome(1,xml,''),('assertion',2))
            baseline={'case':dict(kind='crash',reason='Reviewed trap',patterns=['function signature mismatch'])}
            xml.unlink();text='C:/dev/shandalar-sdl3/src/magic/tests/duel/duel_card_tests.cpp:113: Failure\nExpected equality\nRuntimeError: null function or function signature mismatch'
            kind,count=process_outcome(1,xml,text)
            self.assertEqual((kind,count),('assertion',2))
            self.assertEqual(classify_outcome('case',kind,count,text,baseline),'failed')

    def test_old_explicit_native_seh_baseline_is_adapted_without_mutation(self):
        message='SEH exception with code 0xc0000005 thrown in the test body.'
        record=dict(platform='native',kind='assertion',assertion_count=1,reason='Reviewed native hardware fault',patterns=[re.escape(message)])
        records={'case':record}
        native=expectations_for_platform(records,'native')
        self.assertEqual(classify_outcome('case','crash',0,message,native),'known_failure')
        self.assertEqual(classify_outcome('case','assertion',1,message,native),'failed')
        self.assertEqual(classify_outcome('case','passed',0,'',native),'unexpected_pass')
        self.assertEqual(expectations_for_platform(records,'wasm'),{})
        self.assertEqual(record['kind'],'assertion');self.assertEqual(record['assertion_count'],1)

    def test_native_hardware_fault_is_a_crash_even_with_failure_xml(self):
        with tempfile.TemporaryDirectory() as temporary:
            xml=Path(temporary)/'result.xml'
            root=ET.Element('testsuites',tests='1');case=ET.SubElement(root,'testcase')
            failure=ET.SubElement(case,'failure',message='SEH exception with code 0xc0000005 thrown in the test body.')
            for prefix in ('','unknown file\n'):
                failure.set('message',prefix+'SEH exception with code 0xc0000005 thrown in the test body.')
                ET.ElementTree(root).write(xml)
                self.assertEqual(process_outcome(1,xml,''),('crash',0))
            failure.set('message','Expected equality of these values: power')
            ET.ElementTree(root).write(xml)
            self.assertEqual(process_outcome(1,xml,''),('assertion',1))
            self.assertEqual(process_outcome(1,xml,'DUEL_HARNESS_ERROR: unscripted choice'),('harness',0))
            self.assertEqual(process_outcome(1,xml,'DUEL_HARNESS_ERROR: execution budget exhausted during resolve'),('budget',0))
            xml.unlink()
            self.assertEqual(process_outcome(1,xml,'RuntimeError: memory access out of bounds'),('crash',0))

    def test_platform_scoped_failure_still_requires_wasm_success_and_native_failure(self):
        records = {"case": dict(platform="native", kind="assertion", assertion_count=1,
                                reason="Native null prompt crash", patterns=["access violation"])}
        native = expectations_for_platform(records, "native")
        wasm = expectations_for_platform(records, "wasm")
        self.assertEqual(classify_outcome("case", "assertion", 1, "access violation", native), "known_failure")
        self.assertEqual(classify_outcome("case", "passed", 0, "", native), "unexpected_pass")
        self.assertEqual(classify_outcome("case", "passed", 0, "", wasm), "passed")
        self.assertEqual(classify_outcome("case", "assertion", 1, "access violation", wasm), "failed")
        self.assertEqual(classify_outcome("case", "harness", 0, "access violation", native), "failed")

    def test_unscoped_expectation_applies_to_both_and_bad_platform_is_rejected(self):
        records = {"case": dict(reason="both platforms")}
        for platform in ("native", "wasm"):
            self.assertEqual(expectations_for_platform(records, platform), records)
        with self.assertRaisesRegex(ValueError, "invalid platform"):
            expectations_for_platform({"case": dict(platform="other")}, "native")
        with self.assertRaisesRegex(ValueError, "Unknown test platform"):
            expectations_for_platform(records, "other")

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
