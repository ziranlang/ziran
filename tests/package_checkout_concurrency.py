"""Concurrent commands resolving one locked package share one healthy cache entry.

Each probe process calls PkgCheckout against a local file:// fixture repository
in an empty XDG_CACHE_HOME, as parallel `ziran build` jobs under make -j do.
"""
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

PROBE = Path(sys.argv.pop(1)).resolve()
PROCESSES = 12


def git(*args, cwd):
    return subprocess.run(['git', *args], cwd=cwd, check=True, capture_output=True,
                          text=True).stdout.strip()


class PackageCheckoutConcurrencyTest(unittest.TestCase):
    def setUp(self):
        scratch = tempfile.TemporaryDirectory(prefix='ziran-checkout-')
        self.addCleanup(scratch.cleanup)
        self.root = Path(scratch.name)
        self.env = {k: v for k, v in os.environ.items() if k not in
                    ('DISPLAY', 'WAYLAND_DISPLAY', 'XAUTHORITY', 'DBUS_SESSION_BUS_ADDRESS',
                     'MAKEFLAGS', 'MFLAGS', 'MAKELEVEL')}
        self.env['XDG_CACHE_HOME'] = str(self.root / 'cache')
        self.env['GIT_CONFIG_NOSYSTEM'] = '1'
        self.env['ZIRAN_TEST_ALLOW_LOCAL_GIT'] = '1'
        self.env['HOME'] = str(self.root / 'home')
        (self.root / 'home').mkdir()
        source = self.root / 'source'
        source.mkdir()
        git('init', '-qb', 'master', cwd=source)
        for index in range(3):
            (source / 'src').mkdir(exist_ok=True)
            for name in range(40):
                (source / 'src' / f'part{name}.zi').write_text(f'Value{name} :: {index};\n')
            git('add', '.', cwd=source)
            git('-c', 'user.name=Ziran Test', '-c', 'user.email=test@example.invalid',
                'commit', '-qm', f'fixture {index}', cwd=source)
        self.url = source.as_uri()
        self.commit = git('rev-parse', 'HEAD~1', cwd=source)
        self.sources = self.root / 'cache/ziran/sources'

    def run_all(self, count=PROCESSES):
        jobs = [subprocess.Popen([str(PROBE), self.url, self.commit], env=self.env,
                                 stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
                for _ in range(count)]
        results = [(job.wait(timeout=120), job.stdout.read()) for job in jobs]
        for job in jobs:
            job.stdout.close()
        for code, text in results:
            self.assertEqual(code, 0, text)
        paths = {text.strip() for _, text in results}
        self.assertEqual(len(paths), 1, paths)
        return Path(paths.pop())

    def assert_one_healthy_entry(self, path, ignored=()):
        self.assertEqual(path.parent, self.sources)
        entries = [child.name for child in self.sources.iterdir()
                   if not child.name.endswith('.lock') and child.name not in ignored]
        self.assertEqual(entries, [path.name], 'one entry and no temporary directories')
        self.assertEqual(git('rev-parse', 'HEAD', cwd=path), self.commit)
        self.assertEqual(git('status', '--porcelain', '--untracked-files=all', cwd=path), '')
        self.assertEqual(len(list((path / 'src').iterdir())), 40)

    def test_parallel_fresh_checkouts_publish_one_entry(self):
        path = self.run_all()
        self.assert_one_healthy_entry(path)
        print(f'{PROCESSES} parallel fresh checkouts shared {path.name}', flush=True)

    def test_parallel_repairs_of_one_entry_all_succeed(self):
        path = self.run_all(1)
        for name in range(0, 40, 3):
            (path / 'src' / f'part{name}.zi').unlink()
        self.assertEqual(self.run_all(), path)
        self.assert_one_healthy_entry(path)

    def test_stale_temporary_directory_is_not_an_entry(self):
        path = self.run_all(1)
        stale = path.parent / f'{path.name}.tmp-crashd'
        git('clone', '-q', '--no-checkout', self.url, str(stale), cwd=self.root)
        subprocess.run(['rm', '-rf', str(path)], check=True)
        self.assertEqual(self.run_all(4), path)
        self.assert_one_healthy_entry(path, ignored=(stale.name,))
        self.assertTrue(stale.is_dir(), 'another run never removes a stale sibling')


if __name__ == '__main__':
    unittest.main(verbosity=2)
