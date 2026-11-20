#!/usr/bin/env python3
"""Package the native toolchain from Ziran's completed Gitea check job."""

import argparse
import base64
import gzip
import hashlib
import http.client
import json
import os
import platform
import re
import ssl
import stat
import subprocess
import tarfile
import tempfile
from pathlib import Path
from urllib.parse import urlsplit


HOST = 'git.waozi.xyz'
REPOSITORY = f'https://{HOST}/ziranlang/ziran.git'
TOOLS = ('ziran', 'zi-fmt', 'zi-edit', 'zi2zir', 'zi-api', 'zi-inspect',
         'zi2c', 'zi2go', 'zi2zx', 'zx2wasm', 'zx2native')
PROFILE = re.compile(r'linux-(?:x64|arm64)-glibc-[0-9]+\.[0-9]+')
COMMIT = re.compile(r'[0-9a-f]{40}')


def git(*args):
    return subprocess.check_output(['git', *args])


def clean_revision(commit):
    if not COMMIT.fullmatch(commit) or git('rev-parse', 'HEAD').strip().decode() != commit:
        raise ValueError('toolchain source differs from the requested Git revision')
    source = urlsplit(git('remote', 'get-url', 'origin').strip().decode())
    if (source.scheme != 'https' or source.hostname != HOST or source.port is not None or
            source.path not in ('/ziranlang/ziran', '/ziranlang/ziran.git')):
        raise ValueError('toolchain source must use the canonical Gitea origin')
    if git('status', '--porcelain', '--untracked-files=normal'):
        raise ValueError('toolchain source must be clean')


def host_profile():
    arch = {'x86_64': 'x64', 'aarch64': 'arm64'}.get(platform.machine())
    libc = os.confstr('CS_GNU_LIBC_VERSION') if platform.system() == 'Linux' else None
    if arch is None or libc is None or not re.fullmatch(r'glibc [0-9]+\.[0-9]+', libc):
        raise ValueError('the toolchain registry supports Linux x64 and arm64 with glibc')
    return f'linux-{arch}-{libc.replace(" ", "-")}'


def digest(file):
    with file.open('rb') as stream:
        return hash_stream(stream)


def hash_stream(stream):
    checksum = hashlib.sha256()
    for chunk in iter(lambda: stream.read(65536), b''):
        checksum.update(chunk)
    return checksum.hexdigest()


def write_archive(archive, files):
    with gzip.GzipFile(filename='', fileobj=archive, mode='wb', mtime=0) as compressed:
        with tarfile.open(fileobj=compressed, mode='w|', format=tarfile.PAX_FORMAT) as tar:
            for file in sorted(files):
                if not stat.S_ISREG(file.lstat().st_mode):
                    raise ValueError(f'expected a regular toolchain file: {file}')
                with file.open('rb') as contents:
                    info = os.fstat(contents.fileno())
                    entry = tarfile.TarInfo(file.as_posix())
                    entry.size = info.st_size
                    entry.mode = 0o755 if info.st_mode & stat.S_IXUSR else 0o644
                    tar.addfile(entry, contents)


def pack(commit, output):
    clean_revision(commit)
    profile = host_profile()
    binaries = [Path('build/bin', tool) for tool in TOOLS]
    for binary in binaries:
        if not binary.is_file() or not os.access(binary, os.X_OK):
            raise ValueError(f'missing tested toolchain executable: {binary}')
    subprocess.run([str(binaries[0]), 'version'], check=True)
    sources = git('ls-files', '-z', '--', 'std', 'include', 'templates', 'scripts/ziran_artifacts.py')
    files = binaries + [Path('build/libziran.a')]
    files.extend(Path(os.fsdecode(name)) for name in sources.split(b'\0') if name)
    output.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='.pack-', dir=output) as stage:
        archive = Path(stage, f'{profile}.tar.gz')
        with archive.open('wb') as stream:
            write_archive(stream, files)
        descriptor = archive.with_name(f'{profile}.json')
        metadata = {'version': 1, 'repository': REPOSITORY, 'commit': commit, 'profile': profile,
                    'file': archive.name, 'size': archive.stat().st_size, 'sha256': digest(archive)}
        descriptor.write_text(json.dumps(metadata, sort_keys=True) + '\n')
        clean_revision(commit)
        archive.replace(output / archive.name)
        descriptor.replace(output / descriptor.name)
    print(f'Toolchain artifact {profile} for {commit}')


def read_artifact(descriptor):
    with descriptor.open('rb') as stream:
        contents = stream.read(4097)
    if len(contents) > 4096:
        raise ValueError('toolchain descriptor exceeds 4096 bytes')
    row = json.loads(contents)
    profile = row['profile']
    archive = descriptor.with_name(f'{profile}.tar.gz') if PROFILE.fullmatch(profile) else None
    if (row['version'] != 1 or row['repository'] != REPOSITORY or not COMMIT.fullmatch(row['commit']) or
            archive is None or descriptor.name != f'{profile}.json' or row['file'] != archive.name or
            row['size'] != archive.stat().st_size or row['sha256'] != digest(archive)):
        raise ValueError('toolchain descriptor and archive did not verify')
    return row['commit'], archive


def upload(connection, base, file, authorization):
    path = f'{base}/{file.name}'
    size = file.stat().st_size
    headers = {'Authorization': authorization, 'Content-Length': str(size),
               'Content-Type': 'application/octet-stream'}
    interrupted = None
    try:
        with file.open('rb') as stream:
            connection.request('PUT', path, body=stream, headers=headers)
            with connection.getresponse() as response:
                if response.status == 201:
                    return
                if response.status != 409:
                    raise ValueError(f'Gitea rejected {file.name} (HTTP {response.status})')
    except (BrokenPipeError, ConnectionResetError, http.client.RemoteDisconnected, ssl.SSLEOFError) as error:
        # The server may answer a conflict before reading the complete upload.
        interrupted = error
    finally:
        connection.close()
    # A retry may reuse an immutable file only when the server has the same bytes.
    try:
        connection.request('GET', path, headers={'Authorization': authorization})
        with connection.getresponse() as response:
            if response.status == 404 and interrupted is not None:
                raise interrupted
            if response.status != 200 or int(response.getheader('Content-Length', -1)) != size:
                raise ValueError(f'cannot verify existing Gitea file {file.name}')
            if hash_stream(response) != digest(file):
                raise ValueError(f'Gitea already has different bytes for {file.name}')
    finally:
        connection.close()


def verify(descriptor, expected_commit):
    commit, archive = read_artifact(descriptor)
    if commit != expected_commit:
        raise ValueError('artifact revision differs from the workflow revision')
    return archive


def publish(descriptor, expected_commit):
    archive = verify(descriptor, expected_commit)
    user = os.environ.get('ZIRAN_TOOLCHAIN_PACKAGE_USER')
    token = os.environ.get('ZIRAN_TOOLCHAIN_PACKAGE_TOKEN')
    if not user or not token or ':' in user:
        raise ValueError('provide the Gitea package writer identity and token')
    authorization = 'Basic ' + base64.b64encode(f'{user}:{token}'.encode()).decode()
    base = f'/api/packages/ziranlang/generic/ziran-toolchain/{expected_commit}'
    connection = http.client.HTTPSConnection(HOST, timeout=180)
    upload(connection, base, archive, authorization)
    upload(connection, base, descriptor, authorization)
    print(f'Published {REPOSITORY} toolchain {archive.name} for {expected_commit}')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    subcommands = parser.add_subparsers(dest='command', required=True)
    package = subcommands.add_parser('pack')
    package.add_argument('commit')
    package.add_argument('output', type=Path)
    for command in ('verify', 'publish'):
        artifact = subcommands.add_parser(command)
        artifact.add_argument('commit')
        artifact.add_argument('descriptor', type=Path)
    try:
        args = parser.parse_args()
        if args.command == 'pack':
            pack(args.commit, args.output)
        elif args.command == 'publish':
            publish(args.descriptor, args.commit)
        else:
            print(verify(args.descriptor, args.commit))
    except (OSError, ValueError, KeyError, TypeError, subprocess.CalledProcessError,
            http.client.HTTPException) as error:
        parser.exit(1, f'toolchain package: {error}\n')


if __name__ == '__main__':
    main()
