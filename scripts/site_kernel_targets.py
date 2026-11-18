#!/usr/bin/env python3
"""Validate and copy retained kernel tier receipts without measuring."""
import argparse
import json
import shutil

import site_targets as base
import site_benchmark_cohort as cohort
import site_target_platforms as platforms

OUTPUT = base.SITE / 'generated/kernel-targets'
RECEIPTS = ('wasm-tier-gated-2026-11-18.json',)
TARGETS = ('c', 'go', 'zx', 'native', 'wasm')


def receipts():
    return [(name, json.loads((base.REPO / 'bench/results' / name).read_text()))
            for name in RECEIPTS]


def validate(data):
    assert data['schema_version'] == 1 and data['virtualization'] == 'none'
    assert data['samples'] >= 3 and data['kernels']
    assert data['host_gate'], 'Publish only host-gated kernel measurements'
    assert data['runtimes']['native'] == 'ZX native · translated in process', 'Native must exclude process startup'
    for row in data['kernels'].values():
        assert set(row) == set(TARGETS)
        for measured in row.values():
            assert measured['stdout_verified']
            assert measured['host_gate']['accepted'] and measured['host_gate']['attempts'][-1]['accepted']
            assert len(measured['batch_samples_ms']) == data['samples']
            assert platforms.summarize(measured, measured['result']) == measured


def generate(check=False):
    for name, data in receipts():
        validate(data)
        assert data['engine_policy'] == 'explicit-engine'
        cohort.validate(data)
        source = base.REPO / 'bench/results' / name
        destination = OUTPUT / name
        if check:
            assert destination.read_bytes() == source.read_bytes(), f'Stale kernel receipt: {name}'
        else:
            OUTPUT.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(source, destination)
    if check:
        print('Kernel receipt statistics, cohort and public copies verified')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    generate(parser.parse_args().check)
