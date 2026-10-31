"""Consume dependency-owned artifact descriptors from Python build tools.

Paths belong to the descriptor that declares them. Link metadata is an
optional provider contract; asking for it never reconstructs a source graph.
"""
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import shlex
import subprocess


HEADERS = ('schema', 'package', 'artifact', 'platform', 'key')


def digest(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def _rows(path):
    with Path(path).open(encoding='utf-8') as stream:
        for number, line in enumerate(stream, 1):
            fields = line.rstrip('\n').split('\t')
            if len(fields) != 2 or not all(fields) or any(
                    ord(char) < 32 or ord(char) == 127 for field in fields for char in field):
                raise ValueError(f'malformed artifact row: {path}:{number}')
            yield fields


class Artifact:
    def __init__(self, descriptor, *, artifact=None, platform=None):
        path = Path(descriptor)
        if not path.is_absolute():
            raise ValueError('artifact descriptor must be absolute')
        self.descriptor = path.resolve(strict=True)
        self.root = self.descriptor.parent
        self.rows = {}
        for name, value in _rows(self.descriptor):
            if name in self.rows:
                raise ValueError('duplicate artifact row: ' + name)
            self.rows[name] = value
        if any(name not in self.rows for name in HEADERS) or self.rows['schema'] != '1':
            raise ValueError('unsupported artifact descriptor')
        if not re.fullmatch('[0-9a-f]{64}', self.rows['key']):
            raise ValueError('invalid artifact key')
        if artifact is not None and self.rows['artifact'] != artifact:
            raise ValueError('artifact selection mismatch')
        if platform is not None and self.rows['platform'] != platform:
            raise ValueError('artifact platform mismatch')
        for name, value in self.rows.items():
            if name not in HEADERS:
                output = self.path(name)
                if (not re.fullmatch('[0-9a-f]{64}', value) or not output.is_file()
                        or digest(output) != value):
                    raise ValueError('artifact digest mismatch: ' + name)

    def path(self, name):
        relative = PurePosixPath(name)
        if (not name or '\\' in name or relative.is_absolute() or '..' in relative.parts
                or str(relative) != name or name == '.'):
            raise ValueError('artifact path escapes descriptor or is not canonical: ' + name)
        path = (self.root / name).resolve()
        if not path.is_relative_to(self.root):
            raise ValueError('artifact path escapes descriptor: ' + name)
        return path

    def output(self, name):
        if name in HEADERS or name not in self.rows:
            raise ValueError('undeclared artifact output: ' + name)
        return self.path(name)

    def metadata(self, name):
        """Read a declared JSON object with this artifact's schema and identity."""
        with self.output(name).open(encoding='utf-8') as stream:
            meta = json.load(stream)
        if (not isinstance(meta, dict) or type(meta.get('schema')) is not int
                or meta['schema'] != 1 or any(meta.get(key) != self.rows[key]
                                           for key in ('artifact', 'platform'))):
            raise ValueError('artifact metadata does not match descriptor: ' + name)
        return meta

    def dependencies(self):
        """Direct prerequisites, in the provider's declared edge order."""
        path = self.root / 'dependencies.tsv'
        if not path.exists():
            return
        seen = set()
        for selection, descriptor in _rows(path):
            parts = selection.split(':')
            if len(parts) != 2 or not all(parts) or selection in seen:
                raise ValueError('invalid or duplicate artifact dependency: ' + selection)
            seen.add(selection)
            yield Artifact(descriptor, artifact=parts[1], platform=self.rows['platform'])

    def link_flags(self, *, include_archives=True, env=None, _stack=frozenset()):
        """Expand ordered link.txt rows and prerequisites without shell evaluation.

        include_archives=False omits only this provider's archive rows. Use it
        when the host already compiles those sources through ordinary imports;
        its flags and the prerequisites' foreign archives are still required.
        Shared prerequisites may intentionally appear more than once for static
        link ordering, so only cycles on the current recursion path are rejected.
        """
        if self.descriptor in _stack:
            raise ValueError('artifact dependency cycle')
        stack = _stack | {self.descriptor}
        flags = []
        for kind, value in _rows(self.output('link.txt')):
            if kind == 'archive':
                archive = self.output(value)
                if include_archives:
                    flags.append(str(archive))
            elif kind == 'pkg-config':
                executable = (env if env is not None else os.environ).get('PKG_CONFIG', 'pkg-config')
                flags.extend(shlex.split(subprocess.check_output(
                    [*shlex.split(executable), '--libs', *shlex.split(value)], text=True, env=env)))
            elif kind in ('library', 'flag'):
                flags.append(value)
            elif kind == 'js-library':
                flags.extend(['--js-library', str(self.output(value))])
            elif kind == 'embed':
                name, separator, mount = value.partition('@')
                if (not separator or not mount.startswith('/') or '@' in mount or '\\' in mount
                        or '..' in PurePosixPath(mount).parts or str(PurePosixPath(mount)) != mount):
                    raise ValueError('invalid artifact embed mount: ' + value)
                flags.extend(['--embed-file', str(self.output(name)) + '@' + mount])
            else:
                raise ValueError('unsupported provider link row: ' + kind)
        for child in self.dependencies():
            flags.extend(child.link_flags(env=env, _stack=stack))
        return flags


def request(selection, platform, *, project, ziran='ziran', env=None, options=()):
    """Ask the selected project tool for an artifact and verify its outputs."""
    parts = selection.split(':')
    if len(parts) != 2 or not all(parts):
        raise ValueError('invalid artifact selection: ' + selection)
    project = Path(project).resolve()
    tool = str(ziran)
    if '/' in tool:
        tool = str(Path(tool).absolute())
    locked = [] if (project / 'ziran.local.toml').exists() else ['--locked']
    overrides = [part for value in options for part in ('--option', value)]
    descriptor = subprocess.check_output(
        [tool, 'pkg', 'artifact', selection, '--platform', platform, *overrides, *locked],
        cwd=project, text=True, env=env).strip()
    return Artifact(descriptor, artifact=parts[1], platform=platform)
