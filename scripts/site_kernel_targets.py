#!/usr/bin/env python3
"""Render retained kernel tier receipts; never run measurements during site generation."""
import argparse
import html
import json
from pathlib import Path
import re
import shutil

import site_targets as base
import site_benchmark_cohort as cohort
import site_target_platforms as platforms

OUTPUT = base.SITE / 'generated/kernel-targets'
RECEIPTS = ('wasm-tier-gated-2026-11-16.json',)
MARKER = 'kernel_tiers'
TARGETS = ('c', 'go', 'zx', 'native', 'wasm')


def receipts():
    return [(name, json.loads((base.REPO / 'bench/results' / name).read_text()))
            for name in RECEIPTS]


def validate(data):
    assert data['schema_version'] == 1 and data['virtualization'] == 'none'
    assert data['samples'] >= 3 and data['kernels']
    assert data['host_gate'], 'Publish only host-gated kernel measurements'
    for row in data['kernels'].values():
        assert set(row) == set(TARGETS)
        for measured in row.values():
            assert measured['stdout_verified']
            assert measured['host_gate']['accepted'] and measured['host_gate']['attempts'][-1]['accepted']
            assert len(measured['batch_samples_ms']) == data['samples']
            assert platforms.summarize(measured, measured['result']) == measured


def render():
    sections = []
    for name, data in receipts():
        validate(data)
        assert data['engine_policy'] == 'explicit-engine'
        rows = []
        for kernel, row in data['kernels'].items():
            values = ''.join('<td>' + base.format_microseconds(row[t]['execution_ms']) + '</td>' for t in TARGETS)
            rows.append('<tr><th scope="row">' + html.escape(kernel) + '</th>' + values + '</tr>')
        source = 'generated/kernel-targets/' + name
        sections.append(
            '<article>'
            '<p>Measured ' + html.escape(data['measured_at']) + ' · ' + str(data['samples']) +
            ' samples · checks off · learning off.</p>' +
            '<div class="matrix-scroll" tabindex="0" role="region" aria-label="Kernel results"><table class="target-matrix"><caption class="sr-only">Median µs per kernel call</caption>'
            '<thead><tr><th scope="col">Kernel<small>µs per call</small></th><th scope="col">C · cc -O2</th><th scope="col">Go</th><th scope="col">Instruction engine</th><th scope="col">Native ELF</th><th scope="col">Wasm · V8</th></tr></thead><tbody>' +
            ''.join(rows) + '</tbody></table></div><p><a href="' + source +
            '">Raw samples, exclusions and provenance</a></p></article>')
    content = ('<section class="section" id="kernel-tiers"><p class="eyebrow">Kernel execution tiers</p>'
               '<h2>One source plus a Go reference.</h2>' + cohort.validate(receipts()[0][1]) + ''.join(sections) +
               '<p class="matrix-footnote">Medians use symmetric 1.5×IQR filtering. Compilation is excluded. '
               'C, Go, ZX and Wasm use warm in-process calls. Native ELF runs 32 verified calls per process; '
               'its figures include one process startup divided by 32. Every job runs pinned to one CPU and is '
               'repeated until that CPU held a steady clock at its calibrated sustained speed with the other '
               'hyperthread of its core idle; Go uses GOMAXPROCS=1 there. Recursive Fibonacci with a runtime '
               'input is in the standard suite below. '
               'All tables use the same physical host and compiler revision.</p></section>')
    original = (base.SITE / 'targets.html').read_text()
    pattern = r'(?s)(<!-- generated:' + MARKER + r' -->).*?(<!-- /generated:' + MARKER + r' -->)'
    assert len(re.findall(pattern, original)) == 1, 'Expected one kernel tier marker'
    return re.sub(pattern, lambda match: match[1] + '\n' + content + '\n' + match[2], original)


def generate(check=False):
    expected = render()
    for name, _ in receipts():
        source = base.REPO / 'bench/results' / name
        destination = OUTPUT / name
        if check:
            assert destination.read_bytes() == source.read_bytes(), f'Stale kernel receipt: {name}'
        else:
            OUTPUT.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(source, destination)
    page = base.SITE / 'targets.html'
    if check:
        assert page.read_text() == expected, 'Stale kernel tier table'
        print('Kernel receipt statistics, public copies and generated tables verified')
    else:
        page.write_text(expected)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    generate(parser.parse_args().check)
