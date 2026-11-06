"""Exercise real child-process crashes/timeouts and isolation across batches."""
import json
from pathlib import Path
import sys
import tempfile
from types import SimpleNamespace
import unittest

from batch_cases import case_text, read_journal, run_batch


CHILD = r'''
import json, os, sys, time
from pathlib import Path
import xml.etree.ElementTree as ET
args = dict(arg[2:].split('=', 1) for arg in sys.argv[1:] if '=' in arg)
def path(value):
    return Path(value.replace('/test-output/', os.environ['DUEL_TEST_OUTPUT_DIR'] + '/')
                     .replace('/test-cases/', os.environ.get('DUEL_TEST_CASE_DIR', '') + '/'))
batch = 'duel-case-list' in args
names = path(args['duel-case-list']).read_text().splitlines() if batch else [args['gtest_filter']]
stream = path(args['duel-batch-journal']).open('w') if batch else None
def event(**record):
    if stream:
        stream.write(json.dumps(record) + '\n'); stream.flush()
event(event='protocol', version=1)
failed = False
for name in names:
    print('[ RUN      ] ' + name, flush=True)
    event(event='start', filter=name)
    if failed:
        event(event='end', filter=name, passed=False, skipped=True, failures=[], elapsed_ms=0)
        continue
    if name.endswith('Crash'):
        print('RuntimeError: intentional fake crash', flush=True); os._exit(3)
    if name.endswith('Hang'):
        time.sleep(30)
    if name.rsplit('.', 1)[1].startswith('Slow'):
        time.sleep(0.6)
    failures = ['expected rules failure'] if name.endswith('Fail') or (batch and name.endswith('Leak')) else []
    if failures: print(failures[0], flush=True)
    event(event='end', filter=name, passed=not failures, skipped=False, failures=failures, elapsed_ms=1)
    failed = bool(failures)
    if not batch:
        root=ET.Element('testsuites', tests='1'); node=ET.SubElement(root,'testcase')
        for message in failures: ET.SubElement(node,'failure',message=message)
        ET.ElementTree(root).write(path(args['gtest_output'][4:]))
sys.exit(int(failed))
'''


class BatchTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        child = self.root / 'child.py'
        child.write_text(CHILD)
        self.args = SimpleNamespace(binary=child, node=Path(sys.executable), output=self.root/'results', timeout=1, quiet=True)

    def run_cases(self, names, baseline=None):
        cases = [dict(filter='Behavior.'+name, card=name, csvid=i, fixture='empty', seed=1) for i, name in enumerate(names)]
        return run_batch(self.args, cases, baseline or {}, {}, 0)

    def test_known_failure_skips_are_requeued_and_prior_passes_are_retained(self):
        baseline = {'Behavior.Fail': dict(kind='assertion', assertion_count=1, reason='fake reviewed bug', patterns=['expected rules failure'])}
        results = self.run_cases(['Before', 'Fail', 'After'], baseline)
        self.assertEqual([r['status'] for r in results], ['passed', 'known_failure', 'passed'])
        self.assertEqual(len(list(self.args.output.glob('batch-*.log'))), 2)
        self.assertNotIn('isolated', results[1])

    def test_crash_preserves_preceding_pass_and_restarts_remaining_cases(self):
        results = self.run_cases(['Before', 'Crash', 'After'])
        self.assertEqual([r['failure_kind'] for r in results], ['passed', 'crash', 'passed'])
        self.assertEqual(results[1]['isolated']['failure_kind'], 'crash')

    def test_timeout_is_per_test_and_remaining_tests_still_run(self):
        results = self.run_cases(['Before', 'Hang', 'After'])
        self.assertEqual([r['failure_kind'] for r in results], ['passed', 'timeout', 'passed'])
        self.assertEqual(results[1]['isolated']['failure_kind'], 'timeout')

    def test_failure_that_passes_alone_is_an_isolation_regression(self):
        results = self.run_cases(['Before', 'Leak', 'After'])
        self.assertEqual(results[1]['status'], 'failed')
        self.assertEqual(results[1]['failure_kind'], 'isolation')
        self.assertEqual(results[1]['isolated']['status'], 'passed')
        self.assertEqual(results[2]['status'], 'passed')

    def test_selected_archive_and_asset_cwd_survive_batch_restart_and_isolation(self):
        archive = self.root / 'worker archives' / 'selected cases.bin'
        archive.parent.mkdir(); archive.write_text('selected worker archive')
        self.args.case_data = archive
        (self.root/'asset receipt').write_text('engine asset')
        check = """
assert 'duel-case-data' in args, 'selected archive argument lost'
assert path(args['duel-case-data']).read_text() == 'selected worker archive'
assert Path('asset receipt').read_text() == 'engine asset', 'engine cwd lost'
print('selected archive verified', flush=True)
"""
        self.args.binary.write_text(CHILD.replace("batch = 'duel-case-list' in args", check+"\nbatch = 'duel-case-list' in args"))
        results = self.run_cases(['Before', 'Leak', 'After'])
        self.assertEqual([r['status'] for r in results], ['passed', 'failed', 'passed'])
        self.assertEqual(results[1]['isolated']['status'], 'passed')
        self.assertEqual(len(list(self.args.output.glob('batch-*.log'))), 2)
        for log in list(self.args.output.glob('batch-*.log')) + list((self.args.output/'isolated').glob('*.log')):
            self.assertIn('selected archive verified', log.read_text())

    def test_timeout_does_not_bound_the_combined_batch_duration(self):
        results = self.run_cases(['SlowOne', 'SlowTwo', 'SlowThree'])
        self.assertEqual([r['status'] for r in results], ['passed']*3)

    def test_unexpected_pass_is_confirmed_in_isolation_and_still_fails_gate(self):
        baseline = {'Behavior.Before': dict(kind='assertion', assertion_count=1, reason='old bug', patterns=['expected rules failure'])}
        result = self.run_cases(['Before'], baseline)[0]
        self.assertEqual(result['status'], 'unexpected_pass')
        self.assertEqual(result['isolated']['status'], 'unexpected_pass')

    def test_missing_protocol_is_not_a_successful_empty_run(self):
        self.args.binary.write_text('pass\n')
        with self.assertRaisesRegex(RuntimeError, 'did not execute'):
            self.run_cases(['Before'])

    def test_reusing_output_removes_stale_case_failure_artifacts(self):
        self.assertEqual(self.run_cases(['Fail'])[0]['status'], 'failed')
        self.args.binary.write_text(CHILD.replace("name.endswith('Fail')", 'False'))
        self.assertEqual(self.run_cases(['Fail'])[0]['status'], 'passed')
        for suffix in ('.xml', '.json', '.log'):
            self.assertFalse((self.args.output / ('Behavior.Fail' + suffix)).exists())

    def test_journal_rejects_unknown_duplicate_and_out_of_order_records(self):
        path = self.root/'journal'
        header = json.dumps(dict(event='protocol', version=1))+'\n'
        start = json.dumps(dict(event='start', filter='Behavior.One'))+'\n'
        path.write_text(header + start + '{')
        self.assertEqual(read_journal(path, {'Behavior.One'}), ([], 'Behavior.One'))
        for content in (header+start+start, header+start.replace('One','Other'), '{}\n'):
            path.write_text(content)
            with self.assertRaises(ValueError):
                read_journal(path, {'Behavior.One'})

    def test_console_failures_do_not_bleed_into_neighbors(self):
        text = '[ RUN      ] Behavior.One\nfirst\n[ RUN      ] Behavior.Two\nsecond\n'
        self.assertNotIn('second', case_text(text, 'Behavior.One'))
        self.assertNotIn('first', case_text(text, 'Behavior.Two'))


if __name__ == '__main__':
    unittest.main()
