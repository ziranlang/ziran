"""Check native float casts against truncation and independent range bounds."""
import math
import os
from pathlib import Path
import resource
import signal
import struct
import subprocess
import sys
import tempfile


def run(args):
    subprocess.run(args, check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)


def main():
    ziran = str(Path(sys.argv[1]).resolve())
    repo = Path(__file__).resolve().parents[1]
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    types = [(f'{sign}{width}', sign == 's', width)
             for width in (8, 16, 32, 64) for sign in ('s', 'u')]
    signatures, branches, cases = [], [], []
    for precision in (32, 64):
        for name, signed, width in types:
            function = f'Cast{precision}{name}'
            signatures.append(f'#program_export\n{function} :: (value: float{precision}) -> {name} '
                              f'{{ return cast({name})value }}')
            host_type, specifier = ('int64_t', 'PRId64') if signed else ('uint64_t', 'PRIu64')
            branches.append(f'if (!strcmp(argv[1], "{function}")) '
                            f'{{ printf("%" {specifier} "\\n", ({host_type}){function}(value)); return 0; }}')
            upper = float(1 << (width - int(signed)))
            lower = -upper if signed else 0.0
            significand = 24 if precision == 32 else 53
            candidates = [0.0, -0.0, 1.75, -1.75, lower, lower + 1.0,
                          upper - 1.0, upper * (1.0 - 2.0 ** -significand), upper,
                          lower * (1.0 + 2.0 ** (1 - significand)) if signed else -0.5,
                          math.inf, -math.inf, math.nan]
            for original in candidates:
                value = struct.unpack('f', struct.pack('f', original))[0] if precision == 32 else original
                valid = math.isfinite(value) and lower <= value < upper
                cases.append((function, original, math.trunc(value) if valid else None))
    with tempfile.TemporaryDirectory(prefix='ziran-float-casts-') as directory:
        work = Path(directory)
        source = work / 'casts.zi'
        source.write_text('\n'.join(signatures) + '\n')
        run([ziran, 'ir', '--root', str(work), '-o', str(work / 'ir'), str(source)])
        checked = 0
        for form, root, module in [('source', work, source),
                                   ('saved', work / 'ir', work / 'ir/casts.zir')]:
            for target, compiler, standard, suffix in [
                    ('c', os.environ.get('CC', 'cc'), 'c99', 'c'),
                    ('cpp', os.environ.get('CXX', 'c++'), 'c++17', 'cpp')]:
                output = work / f'{form}-{target}'
                run([ziran, 'build', f'--target={target}', '--no-main', '--root', str(root),
                     '-o', str(output), str(module)])
                header = 'casts.h' if target == 'c' else 'casts.hpp'
                driver = output / f'driver.{suffix}'
                driver.write_text(f'#include "{header}"\n#include <inttypes.h>\n'
                                  '#include <stdio.h>\n#include <stdlib.h>\n#include <string.h>\n'
                                  'int main(int argc, char **argv) {\n'
                                  'if (argc != 3) return 2;\n'
                                  'double value = strtod(argv[2], NULL);\n' +
                                  '\n'.join(branches) + '\nreturn 3;\n}\n')
                executable = output / 'app'
                run([compiler, '-O2', f'-std={standard}', '-fsanitize=undefined',
                     '-fno-sanitize-recover=all', f'-I{repo / "include"}', f'-I{output}',
                     *map(str, output.glob(f'*.{suffix}')), '-lm', '-o', str(executable)])
                for function, value, expected in cases:
                    result = subprocess.run([str(executable), function, repr(value)],
                                            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                    if expected is None:
                        assert result.returncode == -signal.SIGABRT, (form, target, function, value,
                                                                     result.returncode, result.stderr)
                    else:
                        assert result.returncode == 0 and int(result.stdout) == expected, (
                            form, target, function, value, expected, result.returncode,
                            result.stdout, result.stderr)
                    checked += 1
        print(f'{checked} float32/float64 cast cases passed C/C++, source/saved IR and UBSan')


if __name__ == '__main__':
    main()
