#!/usr/bin/env python3
"""Generate integer conversion checks from an independent modulo oracle."""
from pathlib import Path
import sys

TYPES = [(f'{sign}{width}', sign == 's', width)
         for width in (8, 16, 32, 64) for sign in ('s', 'u')]


def wrap(value, signed, width):
    value %= 1 << width
    if signed and value >= 1 << (width - 1):
        value -= 1 << width
    return value


def literal(value):
    if value == -(1 << 63):
        return '(-9223372036854775807 - 1)'
    return f'({value})' if value < 0 else str(value)


def main():
    lines = ['// conformance: numeric.cast.width_pairs.all']
    count = 0
    for index, (source, signed, width) in enumerate(TYPES):
        lines.append(f'Check{index} :: () -> s32 {{')
        maximum = (1 << (width - int(signed))) - 1
        minimum = -(1 << (width - 1)) if signed else 0
        # Include sign boundaries, truncation bits, and nonuniform bit patterns.
        values = sorted({wrap(v, signed, width) for v in
                         (minimum, maximum, -1, 0, 1, 127, 128, 255, 256,
                          0x0102030405060708, 0xFEDCBA9876543210)})
        for vi, value in enumerate(values):
            lines.append(f'    v{vi}: {source} = cast({source}){literal(value)}')
            for destination, dest_signed, dest_width in TYPES:
                count += 1
                expected = wrap(value, dest_signed, dest_width)
                lines.append(f'    if cast({destination})v{vi} != '
                             f'cast({destination}){literal(expected)} {{ return {count} }}')
        lines.append('    return 0\n}')
    lines.extend(['#program_export', 'Answer :: () -> s32 {'])
    for i in range(len(TYPES)):
        lines.append(f'    result{i} := Check{i}()')
        lines.append(f'    if result{i} != 0 {{ return result{i} }}')
    lines.extend(['    return 42', '}', '#program_export',
                  'Run :: () { print("%\\n", Answer()) }'])
    Path(sys.argv[1]).write_text('\n'.join(lines) + '\n')
    print(f'{count} conversions covering all 64 integer type pairs')


if __name__ == '__main__':
    main()
