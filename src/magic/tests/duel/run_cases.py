"""Failure-contained native/Node runner. Known bugs retain their real assertions."""
import argparse
import fnmatch
import json
import os
import re
import subprocess
import sys
import time
from concurrent.futures import ThreadPoolExecutor
import xml.etree.ElementTree as ET
from pathlib import Path

ACCEPTED_STATUSES = ("passed", "known_failure", "unimplemented")


def classify_outcome(name, kind, assertion_count, text, baseline, unimplemented=None):
    missing = (unimplemented or {}).get(name)
    expected = baseline.get(name) or missing
    status = "passed" if kind == "passed" else "failed"
    if expected:
        if not expected.get("reason") or not expected.get("patterns") or (
            expected["kind"] == "assertion" and expected.get("assertion_count", 0) < 1
        ):
            raise ValueError(f"Expectation {name} needs a reason, matching patterns, and a positive count for assertion failures")
        if kind == "passed":
            status = "unexpected_pass"
        elif kind == expected["kind"] and kind != "harness" and all(re.search(p, text) for p in expected["patterns"]) and (
            kind != "assertion" or assertion_count == expected["assertion_count"]
        ):
            status = "unimplemented" if missing else "known_failure"
    return status


def validate_expectations(manifest, baseline, unimplemented):
    cases = {c["filter"]: c for c in manifest["cases"]}
    overlap = set(baseline) & set(unimplemented)
    if overlap:
        raise ValueError(f"Cases appear in both known failures and unimplemented: {sorted(overlap)}")
    for label, records in (("known-failure", baseline), ("unimplemented", unimplemented)):
        unknown = set(records) - set(cases)
        if unknown:
            raise ValueError(f"Stale {label} entries: {sorted(unknown)}")
    for name, record in unimplemented.items():
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


def run_case(args, case, baseline, unimplemented=None):
    name = case["filter"]
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    xml = output / (name + ".xml")
    log = output / (name + ".log")
    xml.unlink(missing_ok=True)
    command = ([str(args.node)] if args.node else []) + [str(args.binary.resolve()),
      "--gtest_filter=" + name, "--gtest_output=xml:" + ("/test-output/" + xml.name if args.node else str(xml))]
    env = dict(os.environ, DUEL_TEST_OUTPUT_DIR=str(output))
    started = time.monotonic()
    assertion_count = 0
    try:
        proc = subprocess.run(command, capture_output=True, text=True, encoding="utf-8", errors="replace",
                              timeout=args.timeout, env=env, cwd=args.binary.resolve().parent)
        text = proc.stdout + proc.stderr
        code = proc.returncode
        if re.search(r"^DUEL_HARNESS_ERROR: execution budget exhausted during [^\r\n]+$", text, re.M):
            kind = "budget"
        elif "DUEL_HARNESS_ERROR:" in text:
            kind = "harness"
        elif code == 0 and xml.exists():
            tree = ET.parse(xml)
            count = int(tree.getroot().get("tests", "0"))
            kind = "passed" if count == 1 and not tree.findall(".//failure") and not tree.findall(".//skipped") else "harness"
        elif xml.exists() and ET.parse(xml).findall(".//failure"):
            kind = "assertion"
            assertion_count = len(ET.parse(xml).findall(".//failure"))
        else:
            kind = "crash"
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
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--baseline", type=Path, required=True)
    parser.add_argument("--unimplemented", type=Path, help="missing-implementation expectations (default: beside --baseline, if present)")
    parser.add_argument("--filter", default="*")
    parser.add_argument("--tier", choices=("behavior", "template", "smoke", "ai", "binding"))
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--timeout", type=float, default=30)
    parser.add_argument("--jobs", type=int, default=1, help="independent test processes (default: 1)")
    parser.add_argument("--quiet", action="store_true", help="print only counts; retain all per-case logs/XML")
    parser.add_argument("--shard-index", type=int, default=0)
    parser.add_argument("--shard-count", type=int, default=1)
    args = parser.parse_args()
    if not 0 <= args.shard_index < args.shard_count:
        parser.error("Invalid shard")
    if args.jobs < 1:
        parser.error("jobs must be positive")
    manifest = json.loads(args.manifest.read_text())
    baseline = json.loads(args.baseline.read_text())["failures"]
    try:
        unimplemented = load_unimplemented(args.baseline, args.unimplemented)
        validate_expectations(manifest, baseline, unimplemented)
    except (OSError, ValueError) as error:
        parser.error(str(error))
    cases = [c for c in manifest["cases"] if fnmatch.fnmatchcase(c["filter"],args.filter) and (not args.tier or c["tier"]==args.tier)]
    cases = cases[args.shard_index::args.shard_count]
    if not cases:
        parser.error("No matching test cases; an empty run is not success")
    # Each child owns its engine state, output files, timeout and crash boundary.
    # map preserves manifest order even when processes complete out of order.
    with ThreadPoolExecutor(max_workers=args.jobs) as executor:
        results = list(executor.map(lambda case: run_case(args, case, baseline, unimplemented), cases))
    summary = {"source":manifest["source"], "results":results,
               "counts":{status:sum(r["status"]==status for r in results) for status in ("passed","known_failure","unimplemented","failed","unexpected_pass")}}
    summary_name = cases[0]["filter"] if len(cases) == 1 else f"shard-{args.shard_index}"
    (args.output / f"summary-{summary_name}.json").write_text(json.dumps(summary,indent=2)+"\n")
    print(json.dumps(summary["counts"]))
    return int(any(r["status"] not in ACCEPTED_STATUSES for r in results))

if __name__ == "__main__":
    sys.exit(main())
