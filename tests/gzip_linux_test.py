#!/usr/bin/env python3
"""Native bounded gzip agrees with Python's independent gzip implementation."""
import gzip
import os
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parent.parent
COMPILER = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / 'build/bin/zi2c'
with tempfile.TemporaryDirectory(prefix='ziran-gzip-') as temporary:
    work = Path(temporary)
    generated = work / 'generated'
    subprocess.run([str(COMPILER), '--no-main', '--root', str(ROOT/'std'),
                    '--module-path', str(ROOT/'std'), '-o', str(generated), str(ROOT/'std/gzip_linux.zi')], check=True)
    executable = work / 'gzip-test'
    subprocess.run([os.environ.get('CC', 'cc'), '-std=c11', '-O1', '-ffunction-sections',
                    '-fdata-sections', '-Wl,--gc-sections', '-I'+str(ROOT/'include'), '-I'+str(generated),
                    str(ROOT/'tests/gzip_linux_client.c'), *map(str, generated.glob('*.c')),
                    '-lz', '-o', str(executable)], check=True)
    plain, encoded, decoded = [work/name for name in ('plain', 'encoded', 'decoded')]
    def run(mode, source, destination, count, valid=True):
        result = subprocess.run([str(executable), mode, str(source), str(destination), str(count)])
        assert result.returncode == (0 if valid else 2), (mode, count, result.returncode)
    for value in [b'', b'{}', b'hello '*12000, bytes(range(256))*256, os.urandom(65536)]:
        plain.write_bytes(value)
        run('compress', plain, encoded, 0)
        assert gzip.decompress(encoded.read_bytes()) == value
        encoded.write_bytes(gzip.compress(value, compresslevel=1))
        run('decompress', encoded, decoded, len(value))
        assert decoded.read_bytes() == value
        if value:
            run('decompress', encoded, decoded, len(value)-1, False)
        original = encoded.read_bytes()
        for invalid in [original[:-1], original+b'x', original+original, original[:-8]+b'\x00'*8]:
            if invalid == original:
                continue
            encoded.write_bytes(invalid)
            run('decompress', encoded, decoded, len(value), False)
    print('Native gzip: independent round trips, exact bounds, CRC, truncation and trailing-data rejection passed')
