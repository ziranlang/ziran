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


if __name__ == "__main__":
    unittest.main()
