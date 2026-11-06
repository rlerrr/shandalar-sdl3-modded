"""Crash-contained batches for run_cases; the flushed journal is authoritative."""
import copy
import json
import os
import re
import subprocess
import time
import xml.etree.ElementTree as ET
from pathlib import Path

from run_cases import ACCEPTED_STATUSES, child_launch, classify_outcome, process_outcome, run_case


def read_journal(path, names):
    """Ignore only a trailing partial write, never malformed completed records."""
    if not path.exists():
        return [], None
    lines = path.read_bytes().splitlines(keepends=True)
    records = [json.loads(line) for line in lines if line.endswith(b"\n")]
    if not records:
        return [], None
    if records.pop(0) != {"event": "protocol", "version": 1}:
        raise ValueError("Unsupported batch journal protocol; rebuild duel_card_tests")
    ended, active, seen = [], None, set()
    for record in records:
        name = record.get("filter")
        if name not in names:
            raise ValueError("Unexpected case in batch journal: " + str(name))
        if record.get("event") == "start" and active is None and name not in seen:
            active = name
            seen.add(name)
        elif record.get("event") == "end" and active == name:
            if not isinstance(record.get("failures"), list) or not all(isinstance(f, str) for f in record["failures"]):
                raise ValueError("Invalid batch failure records")
            if not isinstance(record.get("passed"), bool) or not isinstance(record.get("skipped"), bool):
                raise ValueError("Invalid batch result")
            if not isinstance(record.get("elapsed_ms"), int) or record["elapsed_ms"] < 0:
                raise ValueError("Invalid batch elapsed time")
            if not record["skipped"]:
                if record["passed"] == bool(record["failures"]):
                    raise ValueError("Inconsistent batch pass/failure result")
                ended.append(record)
            active = None
        else:
            raise ValueError("Out-of-order or duplicate batch journal record")
    return ended, active


def case_text(text, name):
    sections = list(re.finditer(r"(?m)^\[ RUN      \] ([^\r\n]+)\r?$", text))
    for i, match in enumerate(sections):
        if match.group(1) == name:
            return text[match.start():sections[i+1].start() if i+1 < len(sections) else len(text)]
    return ""


def execute_batch(args, cases, prefix):
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    listing, journal, log = [output / (prefix + suffix) for suffix in (".cases", ".jsonl", ".log")]
    listing.write_text("".join(c["filter"] + "\n" for c in cases), encoding="utf-8")
    journal.unlink(missing_ok=True)
    child_path = lambda path: "/test-output/" + path.name if args.node else str(path)
    command, env = child_launch(args, output)
    command += ["--gtest_filter=__DUEL_BATCH_REQUIRES_UPDATED_RUNNER__", "--gtest_fail_fast",
               "--duel-case-list=" + child_path(listing), "--duel-batch-journal=" + child_path(journal)]
    started = time.monotonic()
    last_progress, active_since, timed_out = (0, None), started, False
    names = {c["filter"] for c in cases}
    with log.open("w", encoding="utf-8") as stream:
        proc = subprocess.Popen(command, stdout=stream, stderr=subprocess.STDOUT,
                                cwd=args.binary.resolve().parent,
                                env=env)
        try:
            while proc.poll() is None:
                ended, active = read_journal(journal, names)
                progress = (len(ended), active)
                if progress != last_progress:
                    active_since, last_progress = time.monotonic(), progress
                # Initialization and each individual test have separate bounds.
                if time.monotonic() - active_since > args.timeout:
                    timed_out = True
                    proc.kill()
                    break
                time.sleep(0.05)
            proc.wait()
        finally:
            if proc.poll() is None:
                proc.kill()
                proc.wait()
    ended, active = read_journal(journal, names)
    text = log.read_text(encoding="utf-8", errors="replace")
    if not ended and active is None:
        raise RuntimeError(f"Batch did not execute a test; rebuild/check assets. See {log}\n{text[-2000:]}")
    if proc.returncode == 0 and (active or len(ended) != len(cases)):
        raise RuntimeError(f"Successful process omitted batch cases: {log}")
    if active is None and proc.returncode not in (0, 1):
        raise RuntimeError(f"Batch crashed outside a test: {log}")
    if active is None and proc.returncode != 0 and not any(not r["passed"] for r in ended):
        raise RuntimeError(f"Batch crashed outside a test: {log}")
    return ended, active, timed_out, proc.returncode, text, log, journal


def run_batch(args, cases, baseline, unimplemented, batch_index):
    """Stop on the first failure and start the remaining cases in a fresh process."""
    pending = {c["filter"]: c for c in cases}
    results, attempt = [], 0
    while pending:
        prefix = f"batch-{batch_index:05d}-{attempt:03d}"
        ended, active, timeout, code, text, log, journal = execute_batch(args, list(pending.values()), prefix)
        outcomes = ended + ([dict(filter=active, incomplete=True)] if active else [])
        for record in outcomes:
            name = record["filter"]
            case = pending.pop(name)
            detail = case_text(text, name)
            xml = args.output.resolve() / (name + ".xml")
            xml.unlink(missing_ok=True)
            if record.get("incomplete"):
                kind, count = ("timeout", 0) if timeout else process_outcome(code, Path("__missing_batch_xml__"), detail)
            else:
                root = ET.Element("testsuites", tests="1")
                node = ET.SubElement(root, "testcase", name=name)
                for message in record["failures"]:
                    ET.SubElement(node, "failure", message=message).text = message
                # Use the same classifier as isolated execution, including SEH.
                if not record["passed"]:
                    ET.ElementTree(root).write(xml, encoding="utf-8")
                    kind, count = process_outcome(1, xml, detail)
                else:
                    kind, count = "passed", 0
                    if "DUEL_HARNESS_ERROR:" in detail:
                        kind, count = process_outcome(1, Path("__missing_batch_xml__"), detail)
            status = classify_outcome(name, kind, count, detail, baseline, unimplemented)
            result = dict(case, status=status, failure_kind=kind, assertion_count=count,
                          exit_code=code, elapsed_seconds=record.get("elapsed_ms", 0)/1000,
                          log=str(log), journal=str(journal))
            if status != "passed":
                case_log = args.output.resolve() / (name + ".log")
                case_log.write_text(detail, encoding="utf-8")
                result.update(log=str(case_log), xml=str(xml) if xml.exists() else None)
            # A new failure or unexpected pass must reproduce in isolation.
            if status not in ACCEPTED_STATUSES or record.get("incomplete"):
                isolated_args = copy.copy(args)
                isolated_args.output = args.output / "isolated"
                isolated_args.quiet = True
                isolated = run_case(isolated_args, case, baseline, unimplemented)
                result["isolated"] = isolated
                if any(result[key] != isolated[key] for key in ("status", "failure_kind", "assertion_count")):
                    result.update(status="failed", failure_kind="isolation")
            if result["status"] != "passed":
                (args.output / (name + ".json")).write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
            else:
                for suffix in (".json", ".log"):
                    (args.output / (name + suffix)).unlink(missing_ok=True)
            if not args.quiet:
                print(f"{result['status'].upper()}: {name}", flush=True)
            results.append(result)
        attempt += 1
    return results
