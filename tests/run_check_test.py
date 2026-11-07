"""Failure logs stay complete on disk while console excerpts stay bounded."""
import importlib.util
import contextlib
import io
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location("run_check", Path(__file__).with_name("run_check.py"))
runner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runner)


class CheckRunnerTest(unittest.TestCase):
    def test_incremental_selection_failures_bypass_and_corrupt_cache(self):
        scratch = Path(__file__).resolve().parent.parent / 'build/check-logs'
        scratch.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=scratch) as directory:
            repo = Path(directory)
            (repo / 'tests').mkdir()
            logs = repo / 'logs'
            fixture = repo / 'fixture'
            fixture.write_text('one')
            script = repo / 'tests/fixture.sh'
            script.write_text('exit "${FIXTURE_STATUS:-0}"\n')
            other = repo / 'tests/other.sh'
            other.write_text('exit 0\n')
            pins = {script.name: runner.file_hash(script), other.name: runner.file_hash(other)}

            def run(*options):
                argv = ['run_check.py', '--bin-dir', str(repo / 'bin'), '--log-dir', str(logs),
                        '--jobs', '1', '--filter', 'fixture', *options]
                with patch.object(sys, 'argv', argv), contextlib.redirect_stdout(io.StringIO()):
                    code = runner.main()
                return code, json.loads((logs / 'results.json').read_text())

            with patch.object(runner, '__file__', str(repo / 'tests/run_check.py')), \
                    patch.dict(runner.CACHE_SCRIPTS, pins, clear=True), \
                    patch.object(runner, 'input_fingerprint', side_effect=lambda *a: runner.file_hash(fixture)):
                self.assertEqual(run('--incremental')[1]['executed'], 1)
                reused = run('--incremental')[1]
                self.assertEqual((reused['executed'], reused['reused']), (0, 1))
                self.assertEqual(reused['total'], 1)  # Filter never adds the other case.
                fixture.write_text('two')
                self.assertEqual(run('--incremental')[1]['executed'], 1)
                for bypass in ('--no-cache', '--full'):
                    self.assertEqual(run('--incremental', bypass)[1]['executed'], 1)
                self.assertEqual(run()[1]['executed'], 1)
                with patch.dict(os.environ, {'FIXTURE_STATUS': '1'}):
                    for _ in range(2):
                        code, result = run('--incremental')
                        self.assertEqual((code, result['failed'], result['executed']), (1, 1, 1))
                self.assertEqual(run('--incremental')[0], 0)
                with patch.dict(os.environ, {'FIXTURE_STATUS': '1'}):
                    self.assertEqual(run('--no-cache')[0], 1)
                self.assertEqual(run('--incremental')[1]['executed'], 1)
                (logs / 'incremental.json').write_text('{broken')
                self.assertEqual(run('--incremental')[1]['executed'], 1)
                (logs / 'incremental.json').unlink()
                with patch.object(runner, 'input_fingerprint', side_effect=['before', 'after']):
                    self.assertEqual(run('--incremental')[1]['executed'], 1)
                self.assertEqual(json.loads((logs / 'incremental.json').read_text()), {})
                script.write_text('exit 0 # changed, dependency scope needs review\n')
                for _ in range(2):
                    self.assertEqual(run('--incremental')[1]['executed'], 1)

    @unittest.skipUnless(sys.platform.startswith('linux'), 'Linux ELF runtime fingerprint')
    def test_fingerprint_covers_content_runtime_fixtures_environment_and_tools(self):
        scratch = Path(__file__).resolve().parent.parent / 'build/check-logs'
        scratch.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=scratch) as directory:
            repo = Path(directory)
            bin_dir = repo / 'build/bin'
            bin_dir.mkdir(parents=True)
            binary = bin_dir / 'ziran'
            shutil.copyfile(sys.executable, binary)
            source = repo / 'source.zi'
            source.write_text('first')
            (repo / 'tests').mkdir()
            fixture = repo / 'tests/input.zi'
            fixture.write_text('first')
            library = bin_dir.parent / 'libziran.a'
            library.write_bytes(b'runtime one')
            original_run = subprocess.run

            def command(args, **kwargs):
                if args[0] == 'git':
                    return subprocess.CompletedProcess(args, 0, b'source.zi\0', b'')
                return original_run(args, **kwargs)

            env = {'PATH': os.environ['PATH']}
            with patch.object(runner.subprocess, 'run', side_effect=command):
                fingerprint = lambda: runner.input_fingerprint(repo, bin_dir, env)
                previous = fingerprint()
                self.assertIsNotNone(previous)
                self.assertEqual(previous, fingerprint())
                env['INVOCATION_ID'] = 'another command'
                env['HARMONY_BUILD_OWNER'] = 'another lease'
                self.assertEqual(previous, fingerprint())
                for path in (source, fixture, library, binary):
                    stamp = path.stat()
                    with path.open('ab') as output:
                        output.write(b'changed')
                    os.utime(path, ns=(stamp.st_atime_ns, stamp.st_mtime_ns))
                    changed = fingerprint()
                    self.assertIsNotNone(changed)
                    self.assertNotEqual(previous, changed, path)
                    previous = changed
                env['CC'] = 'different toolchain'
                self.assertNotEqual(previous, fingerprint())
                env['ZIRAN_PACKAGE_MAP'] = '/external/package-map'
                self.assertIsNone(fingerprint())
                del env['ZIRAN_PACKAGE_MAP']
                tool = repo / 'tool'
                shutil.copyfile(sys.executable, tool)
                with patch.object(runner.shutil, 'which', return_value=str(tool)):
                    previous = fingerprint()
                    with tool.open('ab') as output:
                        output.write(b'tool changed')
                    self.assertNotEqual(previous, fingerprint())
                with patch.object(runner.shutil, 'which', return_value=None):
                    self.assertIsNone(fingerprint())
                fixture.unlink()
                fixture.symlink_to(source)
                self.assertIsNone(fingerprint())

    def test_unknown_dependency_scope_has_no_cache_key(self):
        self.assertIsNone(runner.cache_key('unknown.sh', ['sh'], Path.cwd(), 'known'))

    def test_large_failure_keeps_all_output_and_only_shows_tail(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            command = [sys.executable, "-c", "import sys; print('first diagnostic'); "
                       "print('line\\n' * 100000, end=''); "
                       "print('final diagnostic', file=sys.stderr); sys.exit(124)"]
            result = runner.run_check("fixture", command, root, {}, root)
            self.assertEqual(result["exit_code"], 124)
            raw = Path(result["log"]).read_text()
            self.assertIn("first diagnostic", raw)
            self.assertIn("final diagnostic", raw)
            excerpt = runner.failure_tail(result["log"])
            self.assertLessEqual(len(excerpt.splitlines()), 60)
            self.assertNotIn("first diagnostic", excerpt)
            self.assertIn("final diagnostic", excerpt)

    def test_missing_command_records_failure_without_stopping_other_checks(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            result = runner.run_check("missing", [str(root / "missing")], root, {}, root)
            self.assertEqual(result["exit_code"], 127)
            self.assertIn("No such file", runner.failure_tail(result["log"]))
            passed = runner.run_check("passed", [sys.executable, "-c", "print('ok')"], root, {}, root)
            self.assertEqual(passed["exit_code"], 0)
            self.assertEqual(Path(passed["log"]).read_text(), "ok\n")

    def test_each_check_gets_a_private_tmpdir_that_is_removed(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            command = [sys.executable, "-c", "import os, tempfile; d = os.environ['TMPDIR']; "
                       "assert os.path.isdir(d); print(d); print(tempfile.mkdtemp())"]
            first = runner.run_check("one", command, root, {}, root, root)
            second = runner.run_check("two", command, root, {}, root, root)
            self.assertEqual((first["exit_code"], second["exit_code"]), (0, 0))
            one = Path(first["log"]).read_text().split()
            two = Path(second["log"]).read_text().split()
            self.assertNotEqual(one[0], two[0])
            self.assertTrue(one[1].startswith(one[0] + "/"))
            self.assertFalse(Path(one[0]).exists() or Path(two[0]).exists())

    def test_jobs_follow_the_build_queue_share(self):
        saved = runner.os.environ.get("BUILD_QUEUE_JOBS")
        try:
            runner.os.environ["BUILD_QUEUE_JOBS"] = "6"
            self.assertEqual(runner.default_jobs(), 6)
            runner.os.environ["BUILD_QUEUE_JOBS"] = ""
            self.assertGreaterEqual(runner.default_jobs(), 1)
        finally:
            if saved is None:
                runner.os.environ.pop("BUILD_QUEUE_JOBS", None)
            else:
                runner.os.environ["BUILD_QUEUE_JOBS"] = saved

    def test_costly_checks_are_balanced_over_shards_and_start_first(self):
        names = ["a.sh", "b.sh", "c.sh", "d.sh", "e.sh", "f.sh", "g.sh"]
        checks = [(name, ["sh", name]) for name in names]
        costs = {"b.sh": 800, "d.sh": 650, "f.sh": 300}
        shards = [[name for name, _ in runner.plan_checks(checks, costs, part, 2)] for part in (1, 2)]
        self.assertEqual(shards, [["b.sh", "a.sh", "e.sh"], ["d.sh", "f.sh", "c.sh", "g.sh"]])
        self.assertEqual(sorted(shards[0] + shards[1]), names)
        whole = [name for name, _ in runner.plan_checks(checks, costs)]
        self.assertEqual(whole, ["b.sh", "d.sh", "f.sh", "a.sh", "c.sh", "e.sh", "g.sh"])

    def test_cost_comes_from_the_script_header(self):
        with tempfile.TemporaryDirectory() as directory:
            script = Path(directory) / "slow.sh"
            script.write_text("#!/bin/sh\n# check-cost: 815\nset -eu\n")
            self.assertEqual(runner.declared_cost(script), 815)
            script.write_text("#!/bin/sh\nset -eu\n")
            self.assertEqual(runner.declared_cost(script), 0)


if __name__ == "__main__":
    unittest.main()
