"""Immutable map selection and atomic publication on generated fixture sources.

The fixtures are disposable files, not source checkouts or Git repositories.
"""
from concurrent.futures import ThreadPoolExecutor
import hashlib
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time
import unittest

PROBE = Path(sys.argv.pop(1)).resolve()


class PackageMapConcurrencyTest(unittest.TestCase):
    def setUp(self):
        self.scratch = tempfile.TemporaryDirectory(prefix='ziran-map-')
        self.addCleanup(self.scratch.cleanup)
        self.root = Path(self.scratch.name)
        (self.root / 'std').mkdir()
        for name in ('left', 'right'):
            folder = self.root / name
            folder.mkdir()
            (folder / 'Value.zi').write_text('Value :: 1;\n')
        self.env = {k: v for k, v in os.environ.items() if k not in
                    ('DISPLAY', 'WAYLAND_DISPLAY', 'XAUTHORITY', 'DBUS_SESSION_BUS_ADDRESS')}
        self.cache = self.root / 'build/.ziran'

    def expected(self, name):
        return f'P\troot\t{self.root}\nM\troot\tValue\t{self.root / name / "Value.zi"}\nP\tstd\t{self.root}\n'.encode()

    def run_probe(self, name, *, success=True):
        result = subprocess.run([str(PROBE), str(self.root), name], env=self.env,
                                capture_output=True, text=True, timeout=15)
        self.assertEqual(result.returncode == 0, success, result.stdout + result.stderr)
        return Path(result.stdout.strip()) if success else None

    def test_a_command_keeps_its_map_after_another_graph_is_published(self):
        left = self.run_probe('left')
        right = self.run_probe('right')
        self.assertNotEqual(left, right)
        self.assertEqual(left.read_bytes(), self.expected('left'))
        self.assertEqual(right.read_bytes(), self.expected('right'))
        self.assertEqual(self.run_probe('left'), left, 'identical graphs reuse one snapshot path')
        self.assertEqual(right.read_bytes(), self.expected('right'))

    def test_parallel_publishers_and_readers_observe_only_complete_selected_maps(self):
        self.run_probe('left')
        allowed = {self.expected('left'), self.expected('right')}
        observations = 0
        with ThreadPoolExecutor(max_workers=8) as pool:
            jobs = [(name, pool.submit(self.run_probe, name)) for name in ('left', 'right') * 16]
            while not all(job.done() for _, job in jobs):
                self.assertIn((self.cache / 'module-map.tsv').read_bytes(), allowed)
                observations += 1
                time.sleep(.001)
            paths = [(name, job.result()) for name, job in jobs]
        for name, path in paths:
            self.assertEqual(path.read_bytes(), self.expected(name))
        self.assertGreater(observations, 0)
        self.assertEqual(len({path for _, path in paths}), 2)
        self.assertFalse(list(self.cache.glob('*.tmp.*')))
        print(f'32 parallel publications; {observations} complete latest-map observations', flush=True)

    def test_failed_snapshot_publication_keeps_the_previous_complete_map(self):
        left = self.run_probe('left')
        digest = hashlib.sha256(self.expected('right')).hexdigest()
        (self.cache / f'module-map.{digest}.tsv').mkdir()
        self.run_probe('right', success=False)
        self.assertEqual(left.read_bytes(), self.expected('left'))
        self.assertEqual((self.cache / 'module-map.tsv').read_bytes(), self.expected('left'))
        self.assertFalse(list(self.cache.glob('*.tmp.*')))

    def test_failed_latest_map_publication_removes_its_temporary_file(self):
        self.cache.mkdir(parents=True)
        (self.cache / 'module-map.tsv').mkdir()
        self.run_probe('left', success=False)
        self.assertFalse(list(self.cache.glob('*.tmp.*')))


if __name__ == '__main__':
    unittest.main(verbosity=2)
