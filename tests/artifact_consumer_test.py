"""Artifact consumption boundaries, ordered static linking and process argv."""
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch


ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('ziran_artifacts', ROOT / 'scripts/ziran_artifacts.py')
artifacts = importlib.util.module_from_spec(spec)
spec.loader.exec_module(artifacts)
PLATFORM = 'linux-x86_64-gnu'


class ConsumerTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.scratch = ROOT / 'work/artifact-consumer-test'
        subprocess.run(['git', 'check-ignore', str(cls.scratch)], cwd=ROOT, check=True,
                       stdout=subprocess.DEVNULL)
        cls.scratch.mkdir(parents=True, exist_ok=True)

    def setUp(self):
        temporary = tempfile.TemporaryDirectory(dir=self.scratch)
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)

    def descriptor(self, files, *, folder='provider', artifact='support', platform=PLATFORM):
        root = self.root / folder
        root.mkdir(parents=True, exist_ok=True)
        rows = dict(schema='1', package='provider', artifact=artifact, platform=platform, key='a' * 64)
        for name, content in files.items():
            path = root / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(content if isinstance(content, bytes) else content.encode())
            rows[name] = artifacts.digest(path)
        descriptor = root / 'artifact.tsv'
        descriptor.write_text(''.join(f'{key}\t{value}\n' for key, value in rows.items()))
        return descriptor

    def test_integrity_identity_and_missing_output(self):
        descriptor = self.descriptor({'data': 'original'})
        artifact = artifacts.Artifact(descriptor, artifact='support', platform=PLATFORM)
        self.assertEqual(artifact.output('data'), descriptor.parent / 'data')
        for expected in (dict(artifact='other'), dict(platform='windows-x86_64-mingw')):
            with self.assertRaisesRegex(ValueError, 'mismatch'):
                artifacts.Artifact(descriptor, **expected)
        artifact.output('data').write_bytes(b'corrupt')
        with self.assertRaisesRegex(ValueError, 'digest'):
            artifacts.Artifact(descriptor)
        artifact.output('data').unlink()
        with self.assertRaisesRegex(ValueError, 'digest'):
            artifacts.Artifact(descriptor)

    def test_descriptor_headers_and_malformed_rows(self):
        descriptor = self.descriptor({'data': 'original'})
        original = descriptor.read_text()
        invalid = [original + 'schema\t1\n', original + 'broken\n',
                   original.replace('schema\t1', 'schema\t2'),
                   original.replace('key\t' + 'a' * 64, 'key\tinvalid'),
                   original.replace('package\tprovider\n', ''), original + 'name\tbad\tvalue\n']
        for content in invalid:
            with self.subTest(content=content[-70:]):
                descriptor.write_text(content)
                with self.assertRaises(ValueError):
                    artifacts.Artifact(descriptor)
        with self.assertRaisesRegex(ValueError, 'absolute'):
            artifacts.Artifact('relative/artifact.tsv')

    def test_paths_and_symlinks_cannot_escape(self):
        artifact = artifacts.Artifact(self.descriptor({'data': 'valid'}))
        for name in ('', '.', '../outside', '/etc/passwd', 'data/../other',
                     'nested//file', './data', 'nested\\file'):
            with self.subTest(name=name), self.assertRaises(ValueError):
                artifact.path(name)
        for name in ('schema', 'missing'):
            with self.assertRaisesRegex(ValueError, 'undeclared'):
                artifact.output(name)
        (artifact.root / 'escape').symlink_to(self.root, target_is_directory=True)
        with self.assertRaisesRegex(ValueError, 'escapes'):
            artifact.path('escape/data')

    def test_declared_symlink_escape_is_rejected(self):
        descriptor = self.descriptor({'data': 'valid'})
        outside = self.root / 'outside'
        outside.write_text('valid')
        (descriptor.parent / 'data').unlink()
        (descriptor.parent / 'data').symlink_to(outside)
        with self.assertRaisesRegex(ValueError, 'escapes'):
            artifacts.Artifact(descriptor)

    def test_json_metadata_identity(self):
        for schema, artifact, platform in ((1, 'support', PLATFORM), (True, 'support', PLATFORM),
                                          (2, 'support', PLATFORM), (1, 'other', PLATFORM),
                                          (1, 'support', 'windows-x86_64-mingw')):
            descriptor = self.descriptor({'metadata.json': json.dumps(dict(
                schema=schema, artifact=artifact, platform=platform, custom=['a', 'b']))})
            current = artifacts.Artifact(descriptor)
            if type(schema) is int and schema == 1 and artifact == 'support' and platform == PLATFORM:
                self.assertEqual(current.metadata('metadata.json')['custom'], ['a', 'b'])
            else:
                with self.assertRaisesRegex(ValueError, 'metadata'):
                    current.metadata('metadata.json')

    def test_order_and_root_archive_omission_preserve_prerequisites(self):
        child = self.descriptor({'child.a': 'child', 'link.txt': 'archive\tchild.a\nlibrary\t-lm\n'},
                                folder='child', artifact='foreign')
        parent = self.descriptor({'second.a': 'second', 'first.a': 'first',
            'link.txt': 'archive\tsecond.a\nflag\t-flto=1\narchive\tfirst.a\n'})
        (parent.parent / 'dependencies.tsv').write_text(f'self:foreign\t{child}\n')
        artifact = artifacts.Artifact(parent)
        foreign = [str(child.parent / 'child.a'), '-lm']
        self.assertEqual(artifact.link_flags(), [str(parent.parent / 'second.a'), '-flto=1',
                                               str(parent.parent / 'first.a'), *foreign])
        self.assertEqual(artifact.link_flags(include_archives=False), ['-flto=1', *foreign])
        (child.parent / 'child.a').write_text('corrupt')
        with self.assertRaisesRegex(ValueError, 'digest'):
            artifact.link_flags()

    def test_dependency_identity_platform_and_duplicates(self):
        child = self.descriptor({'link.txt': 'flag\t-pthread\n'}, folder='child', artifact='foreign')
        parent = self.descriptor({'link.txt': 'flag\t-pthread\n'})
        dependencies = parent.parent / 'dependencies.tsv'
        for content in (f'self:other\t{child}\n', f'bad-selection\t{child}\n',
                        f'self:foreign\t{child}\nself:foreign\t{child}\n', 'self:foreign\trelative\n'):
            dependencies.write_text(content)
            with self.assertRaises(ValueError):
                artifacts.Artifact(parent).link_flags()
        child = self.descriptor({'link.txt': 'flag\t-pthread\n'}, folder='child', artifact='foreign',
                                platform='windows-x86_64-mingw')
        dependencies.write_text(f'self:foreign\t{child}\n')
        with self.assertRaisesRegex(ValueError, 'platform'):
            artifacts.Artifact(parent).link_flags()

    def test_cycles_use_canonical_descriptor_paths(self):
        descriptor = self.descriptor({'link.txt': 'flag\t-pthread\n'})
        alias = self.root / 'alias.tsv'
        alias.symlink_to(descriptor)
        (descriptor.parent / 'dependencies.tsv').write_text(f'self:support\t{alias}\n')
        with self.assertRaisesRegex(ValueError, 'cycle'):
            artifacts.Artifact(descriptor).link_flags()

    def test_shared_prerequisite_keeps_static_link_order(self):
        leaf = self.descriptor({'link.txt': 'library\t-lm\n'}, folder='leaf', artifact='leaf')
        branches = []
        for name in ('left', 'right'):
            branch = self.descriptor({'link.txt': f'flag\t-{name}\n'}, folder=name, artifact=name)
            (branch.parent / 'dependencies.tsv').write_text(f'self:leaf\t{leaf}\n')
            branches.append(f'self:{name}\t{branch}\n')
        root = self.descriptor({'link.txt': 'flag\t-root\n'})
        (root.parent / 'dependencies.tsv').write_text(''.join(branches))
        self.assertEqual(artifacts.Artifact(root).link_flags(), ['-root', '-left', '-lm', '-right', '-lm'])

    def test_link_resources_and_pkg_config_are_argv(self):
        descriptor = self.descriptor({'web glue.js': 'glue', 'bundle.zx': 'bundle',
            'link.txt': 'js-library\tweb glue.js\nembed\tbundle.zx@/program.zx\n'
                        'pkg-config\tstatic-lib \'name with spaces\'\n'})
        env = dict(PKG_CONFIG="'/tool path/pkg-config' --static")
        with patch.object(artifacts.subprocess, 'check_output', return_value="'-Lwith spaces' -lvalue\n") as run:
            self.assertEqual(artifacts.Artifact(descriptor).link_flags(env=env),
                ['--js-library', str(descriptor.parent / 'web glue.js'), '--embed-file',
                 str(descriptor.parent / 'bundle.zx') + '@/program.zx', '-Lwith spaces', '-lvalue'])
        self.assertEqual(run.call_args.args[0], ['/tool path/pkg-config', '--static', '--libs',
                                               'static-lib', 'name with spaces'])
        self.assertEqual(run.call_args.kwargs['env'], env)

    def test_unknown_link_rows_and_invalid_mounts_fail(self):
        for row in ('future\tignored', 'embed\tdata@../outside', 'embed\tdata@/../outside',
                    'embed\tdata@/first@/second', 'embed\tdata@/first/./second',
                    'embed\tdata@/first\\second', 'archive\tmissing'):
            descriptor = self.descriptor({'data': 'data', 'link.txt': row + '\n'})
            with self.subTest(row=row), self.assertRaises(ValueError):
                artifacts.Artifact(descriptor).link_flags()

    def test_request_locks_options_and_selected_tool_without_shell(self):
        descriptor = self.descriptor({'data': 'data'})
        options = ['provider:mode=value with spaces;$unexpanded', 'support:sdk.path=/a=b']
        env = dict(PATH='/tools')
        with patch.object(artifacts.subprocess, 'check_output', return_value=str(descriptor) + '\n') as run:
            artifacts.request('provider:support', PLATFORM, project=self.root,
                              ziran='/tools/my ziran', env=env, options=options)
            self.assertEqual(run.call_args.args[0], ['/tools/my ziran', 'pkg', 'artifact',
                'provider:support', '--platform', PLATFORM, '--option', options[0],
                '--option', options[1], '--locked'])
            self.assertEqual(run.call_args.kwargs['env'], env)
            (self.root / 'ziran.local.toml').write_text('[overrides]\n')
            artifacts.request('provider:support', PLATFORM, project=self.root)
            self.assertNotIn('--locked', run.call_args.args[0])
            before = run.call_count
            with self.assertRaisesRegex(ValueError, 'selection'):
                artifacts.request('malformed', PLATFORM, project=self.root)
            self.assertEqual(run.call_count, before)

    @unittest.skipUnless(os.name == 'posix', 'native fixture uses the host C toolchain')
    def test_real_foreign_static_link_from_ordered_metadata(self):
        sources = {'main': 'int first(void); int main(void) { return first() != 42; }\n',
                   'first': 'int second(void); int first(void) { return second(); }\n',
                   'second': 'int second(void) { return 42; }\n'}
        for name, content in sources.items():
            source = self.root / (name + '.c')
            source.write_text(content)
            if name != 'main':
                obj = self.root / (name + '.o')
                subprocess.run(['cc', '-c', str(source), '-o', str(obj)], check=True)
                subprocess.run(['ar', 'rcs', str(self.root / (name + '.a')), str(obj)], check=True)
        descriptor = self.descriptor({'first.a': (self.root / 'first.a').read_bytes(),
            'second.a': (self.root / 'second.a').read_bytes(),
            'link.txt': 'archive\tfirst.a\narchive\tsecond.a\n'})
        output = self.root / 'foreign-consumer'
        subprocess.run(['cc', str(self.root / 'main.c'),
                        *artifacts.Artifact(descriptor).link_flags(), '-o', str(output)], check=True)
        subprocess.run([str(output)], check=True)


if __name__ == '__main__':
    unittest.main()
