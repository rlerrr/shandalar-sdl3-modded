"""Failure-contained native/Node runner. Known bugs retain their real assertions."""
import argparse
import fnmatch
import json
import os
import re
import subprocess
import sys
import time
from concurrent.futures import ThreadPoolExecutor, as_completed
import xml.etree.ElementTree as ET
from pathlib import Path, PurePosixPath

ACCEPTED_STATUSES = ("passed", "known_failure", "unimplemented")


def expectations_for_platform(records, platform):
    if platform not in ("native", "wasm"):
        raise ValueError("Unknown test platform: " + platform)
    for name, record in records.items():
        if record.get("platform", platform) not in ("native", "wasm"):
            raise ValueError(f"Expectation {name} has an invalid platform")
    selected = {name: record for name, record in records.items()
                if record.get("platform", platform) == platform}
    # Older native-only baselines serialized caught hardware exceptions as one
    # assertion. Adapt only explicitly pinned SEH signatures, preserving their
    # on-disk identity, exact patterns and unexpected-pass checks.
    seh_prefix = re.escape("SEH exception with code 0xc0000005 thrown in the test body.")
    return {name: dict(record, kind="crash", assertion_count=0)
            if platform == "native" and record.get("platform") == "native"
            and record.get("kind") == "assertion" and record.get("assertion_count") == 1
            and any(pattern.startswith(seh_prefix) for pattern in record.get("patterns", []))
            else record for name, record in selected.items()}


def expectation_pattern_counts(name, record):
    counts = record.get("pattern_counts")
    if "pattern_counts" in record and (
            not isinstance(counts, list) or len(counts) != len(record.get("patterns", []))
            or any(type(count) is not int or count < 1 for count in counts)):
        raise ValueError(f"Expectation {name} needs one positive occurrence count per pattern")
    return counts


def classify_outcome(name, kind, assertion_count, text, baseline, unimplemented=None):
    missing = (unimplemented or {}).get(name)
    expected = baseline.get(name) or missing
    status = "passed" if kind == "passed" else "failed"
    if expected:
        if not expected.get("reason") or not expected.get("patterns") or (
            expected["kind"] == "assertion" and expected.get("assertion_count", 0) < 1
        ):
            raise ValueError(f"Expectation {name} needs a reason, matching patterns, and a positive count for assertion failures")
        counts = expectation_pattern_counts(name, expected)
        if kind == "passed":
            status = "unexpected_pass"
        elif kind == expected["kind"] and kind != "harness" and all(
            re.search(pattern, text) if counts is None else sum(1 for _ in re.finditer(pattern, text)) == counts[index]
            for index, pattern in enumerate(expected["patterns"])) and (
            kind != "assertion" or assertion_count == expected["assertion_count"]
        ):
            status = "unimplemented" if missing else "known_failure"
    return status


def partial_manifest_sources(manifest):
    """Only an explicit, complete source-file selection relaxes stale-name checks.

    This is an authoring gate for the selected files. The ordinary full manifest
    remains the authority for stale expectations in the rest of the catalog.
    """
    if "selection" not in manifest:
        return None
    selection = manifest["selection"]
    if (not isinstance(selection, dict) or set(selection) != {"version", "complete", "source_files"}
            or type(selection["version"]) is not int or selection["version"] != 1
            or selection["complete"] is not False):
        raise ValueError("Invalid partial manifest selection metadata")
    sources = selection["source_files"]
    if not isinstance(sources, list) or not sources or any(
            not isinstance(source, str) or "\\" in source
            or PurePosixPath(source).as_posix() != source
            or not source.startswith("cases/") or not source.endswith(".json")
            or ".." in PurePosixPath(source).parts for source in sources):
        raise ValueError("Invalid partial manifest source files")
    if len(set(sources)) != len(sources):
        raise ValueError("Duplicate partial manifest source files")
    if not manifest.get("cases") or any(case.get("tier") != "behavior"
            or case.get("source_file") not in sources for case in manifest["cases"]):
        raise ValueError("Partial manifest must contain only selected behavior cases")
    if {case["source_file"] for case in manifest["cases"]} != set(sources):
        raise ValueError("Partial manifest contains an empty selected source file")
    return set(sources)


def validate_expectation_signature(name, record):
    if (not isinstance(record, dict) or record.get("kind") not in ("assertion", "crash", "timeout", "budget")
            or not isinstance(record.get("reason"), str) or not record["reason"].strip()
            or not isinstance(record.get("patterns"), list) or not record["patterns"]
            or any(not isinstance(pattern, str) or not pattern for pattern in record["patterns"])
            or (record["kind"] == "assertion" and (type(record.get("assertion_count")) is not int
                                                    or record["assertion_count"] < 1))):
        raise ValueError(f"Expectation {name} needs a reason, matching patterns, and a positive count for assertion failures")
    expectation_pattern_counts(name, record)
    try:
        for pattern in record["patterns"]:
            re.compile(pattern)
    except re.error as error:
        raise ValueError(f"Expectation {name} has an invalid pattern: {error}") from error


def validate_expectations(manifest, baseline, unimplemented):
    partial = partial_manifest_sources(manifest) is not None
    expectations_for_platform(baseline, "native")
    expectations_for_platform(unimplemented, "native")
    cases = {c["filter"]: c for c in manifest["cases"]}
    overlap = set(baseline) & set(unimplemented)
    if overlap:
        raise ValueError(f"Cases appear in both known failures and unimplemented: {sorted(overlap)}")
    for label, records in (("known-failure", baseline), ("unimplemented", unimplemented)):
        unknown = set(records) - set(cases)
        if unknown and not partial:
            raise ValueError(f"Stale {label} entries: {sorted(unknown)}")
        for name, record in records.items():
            if label == "unimplemented" and name in cases and (
                    record.get("handler") != "card_dummy" or record.get("kind") != "assertion"):
                raise ValueError(f"Unimplemented case {name} must document missing card_dummy behavior with assertions")
            validate_expectation_signature(name, record)
            if name in cases:
                for key in ("card", "csvid", "oracle_id", "handler"):
                    if key in record and record[key] != cases[name].get(key):
                        raise ValueError(f"Stale {label} entry {name}: {key} changed")
    for name, record in unimplemented.items():
        if name not in cases:
            continue  # Explicit partial manifest: this expectation belongs to another source.
        case = cases[name]
        if record.get("handler") != "card_dummy" or record.get("kind") != "assertion":
            raise ValueError(f"Unimplemented case {name} must document missing card_dummy behavior with assertions")
        for key in ("card", "csvid", "oracle_id", "handler"):
            if record.get(key) != case.get(key):
                raise ValueError(f"Stale unimplemented entry {name}: {key} changed; regenerate the manifest and review implementation status")


def load_unimplemented(baseline_path, explicit_path=None):
    path = explicit_path or baseline_path.with_name("unimplemented.json")
    if explicit_path is None and not path.exists():
        return {}
    data = json.loads(path.read_text(encoding="utf-8"))
    if data.get("version") != 1 or not isinstance(data.get("cases"), dict):
        raise ValueError(f"Unsupported unimplemented schema: {path}")
    return data["cases"]


def child_launch(args, output):
    """Use one immutable engine with a caller-owned archive on either platform."""
    command = ([str(args.node)] if args.node else []) + [str(args.binary.resolve())]
    env = dict(os.environ, DUEL_TEST_OUTPUT_DIR=str(output))
    case_data = getattr(args, "case_data", None)
    if case_data is not None:
        case_data = case_data.resolve()
        if not case_data.is_file():
            raise ValueError(f"Case archive does not exist: {case_data}")
        if args.node:
            env["DUEL_TEST_CASE_DIR"] = str(case_data.parent)
            archive_path = "/test-cases/" + case_data.name
        else:
            archive_path = str(case_data)
        command.append("--duel-case-data=" + archive_path)
    return command, env


def process_outcome(code, xml, text):
    if re.search(r"^DUEL_HARNESS_ERROR: execution budget exhausted during [^\r\n]+$", text, re.M):
        return "budget", 0
    if "DUEL_HARNESS_ERROR:" in text:
        return "harness", 0
    if xml.exists():
        tree = ET.parse(xml)
        failures = tree.findall(".//failure")
        # GoogleTest catches native hardware faults and writes failure XML.
        # Keep these as crashes, just like Wasm traps, rather than rules assertions.
        if any(re.search(r"(?m)^SEH exception with code 0x[0-9a-fA-F]+ thrown in the test body\.$",
                        f.get("message", "")) for f in failures):
            # Earlier rules assertions must not disappear behind a known crash.
            return ("crash", 0) if len(failures) == 1 else ("assertion", len(failures))
        if code == 0:
            count = int(tree.getroot().get("tests", "0"))
            return ("passed" if count == 1 and not failures and not tree.findall(".//skipped") else "harness"), 0
        if failures:
            return "assertion", len(failures)
    # A trap or fatal engine error prevents GoogleTest from flushing XML.
    # Preserve earlier rules failures in either native or Wasm console format.
    if "RuntimeError:" in text or "DUEL_ENGINE_ERROR:" in text:
        prior_assertions = re.findall(
            r"(?m)^[^\r\n]*duel_card_tests\.cpp(?::\d+: Failure|\(\d+\): error:)[^\r\n]*$", text)
        if prior_assertions:
            return "assertion", len(prior_assertions) + 1
    return "crash", 0


def run_case(args, case, baseline, unimplemented=None):
    platform = "wasm" if args.node else "native"
    baseline = expectations_for_platform(baseline, platform)
    unimplemented = expectations_for_platform(unimplemented or {}, platform)
    name = case["filter"]
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    xml = output / (name + ".xml")
    log = output / (name + ".log")
    xml.unlink(missing_ok=True)
    command, env = child_launch(args, output)
    command += ["--gtest_filter=" + name,
                "--gtest_output=xml:" + ("/test-output/" + xml.name if args.node else str(xml))]
    started = time.monotonic()
    assertion_count = 0
    try:
        proc = subprocess.run(command, capture_output=True, text=True, encoding="utf-8", errors="replace",
                              timeout=args.timeout, env=env, cwd=args.binary.resolve().parent)
        text = proc.stdout + proc.stderr
        code = proc.returncode
        kind, assertion_count = process_outcome(code, xml, text)
    except subprocess.TimeoutExpired as error:
        def decode(s): return s.decode("utf-8", errors="replace") if isinstance(s, bytes) else s or ""
        text, code, kind = decode(error.stdout) + decode(error.stderr), None, "timeout"
    except (OSError, ET.ParseError) as error:
        text, code, kind = str(error), None, "harness"
    log.write_text(text, encoding="utf-8")
    expected = baseline.get(name) or (unimplemented or {}).get(name)
    status = classify_outcome(name, kind, assertion_count, text, baseline, unimplemented)
    result = dict(case, status=status, failure_kind=kind, exit_code=code,
                  assertion_count=assertion_count,
                  elapsed_seconds=round(time.monotonic()-started, 3), log=str(log), xml=str(xml))
    (output / (name + ".json")).write_text(json.dumps(result, indent=2) + "\n")
    if not getattr(args, "quiet", False):
        print(f"{status.upper()}: {name} — {case['card']} (CSVID {case['csvid']}) fixture={case['fixture']} seed={case['seed']}")
        if status != "passed" and case.get("source_file"):
            print("Case file: " + case["source_file"])
        if status not in ACCEPTED_STATUSES:
            print(text[-12000:])
        elif status == "unimplemented":
            print("Missing implementation: " + expected["reason"])
        elif status == "known_failure":
            print("Known bug: " + expected["reason"])
    return result

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--node", type=Path)
    parser.add_argument("--case-data", type=Path, help="external case archive on the host (native or Node/Wasm)")
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--baseline", type=Path, required=True)
    parser.add_argument("--unimplemented", type=Path, help="missing-implementation expectations (default: beside --baseline, if present)")
    parser.add_argument("--filter", default="*")
    parser.add_argument("--tier", choices=("behavior", "template", "smoke", "ai", "binding"))
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--timeout", type=float, default=30)
    parser.add_argument("--jobs", type=int, default=1, help="independent test processes (default: 1)")
    parser.add_argument("--batch-size", type=int, default=1, help="cases per initialized process (default: 1, isolated)")
    parser.add_argument("--quiet", action="store_true", help="print counts and progress; retain diagnostic artifacts")
    parser.add_argument("--shard-index", type=int, default=0)
    parser.add_argument("--shard-count", type=int, default=1)
    args = parser.parse_args()
    if not 0 <= args.shard_index < args.shard_count:
        parser.error("Invalid shard")
    if args.jobs < 1 or args.batch_size < 1 or args.timeout <= 0:
        parser.error("jobs, batch-size and timeout must be positive")
    manifest = json.loads(args.manifest.read_text())
    baseline = json.loads(args.baseline.read_text())["failures"]
    try:
        unimplemented = load_unimplemented(args.baseline, args.unimplemented)
        validate_expectations(manifest, baseline, unimplemented)
        child_launch(args, args.output.resolve())
    except (OSError, ValueError) as error:
        parser.error(str(error))
    cases = [c for c in manifest["cases"] if fnmatch.fnmatchcase(c["filter"],args.filter) and (not args.tier or c["tier"]==args.tier)]
    cases = cases[args.shard_index::args.shard_count]
    if not cases:
        parser.error("No matching test cases; an empty run is not success")
    if len({case["filter"] for case in cases}) != len(cases):
        parser.error("Duplicate test names in the selected manifest cases")
    args.output.mkdir(parents=True, exist_ok=True)
    started = time.monotonic()
    platform = "wasm" if args.node else "native"
    baseline = expectations_for_platform(baseline, platform)
    unimplemented = expectations_for_platform(unimplemented, platform)
    results = []
    if args.batch_size == 1:
        with ThreadPoolExecutor(max_workers=args.jobs) as executor:
            results = list(executor.map(lambda case: run_case(args, case, baseline, unimplemented), cases))
    else:
        from batch_cases import run_batch
        batches = [cases[i:i+args.batch_size] for i in range(0, len(cases), args.batch_size)]
        last_update = started
        with ThreadPoolExecutor(max_workers=args.jobs) as executor:
            futures = [executor.submit(run_batch, args, batch, baseline, unimplemented, i) for i, batch in enumerate(batches)]
            try:
                for future in as_completed(futures):
                    results.extend(future.result())
                    if time.monotonic() - last_update >= 10 or len(results) == len(cases):
                        progress = dict(completed=len(results), total=len(cases), elapsed_seconds=round(time.monotonic()-started, 1),
                                        unexpected=sum(r["status"] not in ACCEPTED_STATUSES for r in results))
                        (args.output / "progress.json").write_text(json.dumps(progress)+"\n")
                        print(json.dumps(progress), file=sys.stderr, flush=True)
                        last_update = time.monotonic()
            except BaseException:
                for future in futures:
                    future.cancel()
                raise
        order = {case["filter"]: i for i, case in enumerate(cases)}
        results.sort(key=lambda result: order[result["filter"]])
    summary = {"source":manifest["source"], "results":results,
               "batch_size":args.batch_size, "jobs":args.jobs, "elapsed_seconds":round(time.monotonic()-started, 3),
               "counts":{status:sum(r["status"]==status for r in results) for status in ("passed","known_failure","unimplemented","failed","unexpected_pass")}}
    if "selection" in manifest:
        summary["selection"] = manifest["selection"]
    if args.case_data is not None:
        summary["case_data"] = str(args.case_data.resolve())
    summary_name = cases[0]["filter"] if len(cases) == 1 else f"shard-{args.shard_index}"
    (args.output / f"summary-{summary_name}.json").write_text(json.dumps(summary,indent=2)+"\n")
    print(json.dumps(summary["counts"]))
    return int(any(r["status"] not in ACCEPTED_STATUSES for r in results))

if __name__ == "__main__":
    sys.exit(main())
