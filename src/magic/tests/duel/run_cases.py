"""Failure-contained native/Node runner. Known bugs retain their real assertions."""
import argparse
import fnmatch
import json
import os
import re
import subprocess
import sys
import time
import xml.etree.ElementTree as ET
from pathlib import Path

def run_case(args, case, baseline):
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
        if "DUEL_HARNESS_ERROR:" in text:
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
    expected = baseline.get(name)
    status = "passed" if kind == "passed" else "failed"
    if expected:
        if not expected.get("reason") or not expected.get("patterns") or (
            expected["kind"] == "assertion" and expected.get("assertion_count", 0) < 1
        ):
            raise ValueError(f"Known failure {name} needs a reason, matching patterns, and a positive count for assertion failures")
        if kind == "passed":
            status = "unexpected_pass"
        elif kind == expected["kind"] and kind != "harness" and all(re.search(p, text) for p in expected["patterns"]) and (
            kind != "assertion" or assertion_count == expected["assertion_count"]
        ):
            status = "known_failure"
    result = dict(case, status=status, failure_kind=kind, exit_code=code,
                  assertion_count=assertion_count,
                  elapsed_seconds=round(time.monotonic()-started, 3), log=str(log), xml=str(xml))
    (output / (name + ".json")).write_text(json.dumps(result, indent=2) + "\n")
    print(f"{status.upper()}: {name} — {case['card']} (CSVID {case['csvid']}) fixture={case['fixture']} seed={case['seed']}")
    if status != "passed" and case.get("source_file"):
        print("Case file: " + case["source_file"])
    if status not in ("passed", "known_failure"):
        print(text[-12000:])
    elif status == "known_failure":
        print("Known bug: " + expected["reason"])
    return result

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--node", type=Path)
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--baseline", type=Path, required=True)
    parser.add_argument("--filter", default="*")
    parser.add_argument("--tier", choices=("behavior", "template", "smoke", "ai"))
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--timeout", type=float, default=30)
    parser.add_argument("--shard-index", type=int, default=0)
    parser.add_argument("--shard-count", type=int, default=1)
    args = parser.parse_args()
    if not 0 <= args.shard_index < args.shard_count:
        parser.error("Invalid shard")
    manifest = json.loads(args.manifest.read_text())
    baseline = json.loads(args.baseline.read_text())["failures"]
    cases = [c for c in manifest["cases"] if fnmatch.fnmatchcase(c["filter"],args.filter) and (not args.tier or c["tier"]==args.tier)]
    cases = cases[args.shard_index::args.shard_count]
    if not cases:
        parser.error("No matching test cases; an empty run is not success")
    unknown = set(baseline) - {c["filter"] for c in manifest["cases"]}
    if unknown:
        parser.error(f"Stale known-failure entries: {sorted(unknown)}")
    results = [run_case(args,case,baseline) for case in cases]
    summary = {"source":manifest["source"], "results":results,
               "counts":{status:sum(r["status"]==status for r in results) for status in ("passed","known_failure","failed","unexpected_pass")}}
    summary_name = cases[0]["filter"] if len(cases) == 1 else f"shard-{args.shard_index}"
    (args.output / f"summary-{summary_name}.json").write_text(json.dumps(summary,indent=2)+"\n")
    print(json.dumps(summary["counts"]))
    return int(any(r["status"] not in ("passed","known_failure") for r in results))

if __name__ == "__main__":
    sys.exit(main())
