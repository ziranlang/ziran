"""Failure logs stay complete on disk while console excerpts stay bounded."""
import importlib.util
from pathlib import Path
import sys
import tempfile
import unittest

spec = importlib.util.spec_from_file_location("run_check", Path(__file__).with_name("run_check.py"))
runner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runner)


class CheckRunnerTest(unittest.TestCase):
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
