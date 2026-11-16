#!/usr/bin/env python3
"""Render retained kernel tier receipts; never run measurements during site generation."""
import argparse
import html
import json
from pathlib import Path
import re
import shutil

import site_targets as base
import site_target_platforms as platforms

OUTPUT = base.SITE / 'generated/kernel-targets'
RECEIPTS = (
    'wasm-tier-omega-2026-11-16-explicit-engine.json',
    'wasm-tier-alpha-2026-11-16-fields-after.json',
)
MARKER = 'kernel_tiers'
TARGETS = ('c', 'zx', 'native', 'wasm')


def receipts():
    return [(name, json.loads((base.REPO / 'bench/results' / name).read_text()))
            for name in RECEIPTS]


def validate(data):
    assert data['schema_version'] == 1 and data['virtualization'] == 'none'
    assert data['samples'] >= 3 and data['kernels']
    for row in data['kernels'].values():
        assert set(row) == set(TARGETS)
        for measured in row.values():
            assert measured['stdout_verified']
            assert len(measured['batch_samples_ms']) == data['samples']
            assert platforms.summarize(measured, measured['result']) == measured


def render():
    sections = []
    for name, data in receipts():
        validate(data)
        explicit = data.get('engine_policy') == 'explicit-engine'
        # Historic receipts used BundleInstantiate(Auto), which selected native
        # on these Linux hosts. Do not turn that measurement into an engine claim.
        zx_label = 'Instruction engine' if explicit else 'ZX Auto (native)'
        rows = []
        for kernel, row in data['kernels'].items():
            values = ''.join('<td>' + base.format_time(row[t]['execution_ms']) + '</td>' for t in TARGETS)
            rows.append('<tr><th scope="row">' + html.escape(kernel) + '</th>' + values + '</tr>')
        source = 'generated/kernel-targets/' + name
        compiler = 'with uncommitted compiler changes' if data.get('compiler_dirty') else 'from a clean compiler checkout'
        checks = ('off' if data.get('checks') is False else
                  'off (manual-safety default)' if 'unchecked' in name else 'on (historical build)')
        caveat = ('' if explicit else '<p>The historical receipt called this ZX column the instruction engine, '
                  'but its Auto policy selected native on Linux. An explicit engine measurement is pending.</p>')
        sections.append(
            '<article><h3>' + html.escape(data['host']) + '</h3>'
            '<p>Measured ' + html.escape(data['measured_at']) + ' · ' + str(data['samples']) +
            ' samples · checks ' + checks + ' · ' + compiler + '.</p>' + caveat +
            '<div class="matrix-scroll" tabindex="0" role="region" aria-label="' + html.escape(data['host']) +
            ' kernel results"><table class="target-matrix"><caption class="sr-only">Median time per kernel call</caption>'
            '<thead><tr><th scope="col">Kernel</th><th scope="col">C · cc -O2</th><th scope="col">' + zx_label +
            '</th><th scope="col">Native ELF</th><th scope="col">Wasm · V8</th></tr></thead><tbody>' +
            ''.join(rows) + '</tbody></table></div><p><a href="' + source +
            '">Raw samples, exclusions and provenance</a></p></article>')
    content = ('<section class="section" id="kernel-tiers"><p class="eyebrow">Kernel execution tiers</p>'
               '<h2>Omega and Alpha, measured separately.</h2>' + ''.join(sections) +
               '<p class="matrix-footnote">Medians use symmetric 1.5×IQR filtering. Compilation is excluded. '
               'C, ZX and Wasm use warm in-process calls. Native ELF runs 32 verified calls per process; '
               'its figures include one process startup divided by 32. Calls can be constant-folded, '
               'so tiny results measure different amounts of remaining work. Measurement times and '
               'compiler revisions are recorded separately; compare targets within one host and receipt.</p></section>')
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
