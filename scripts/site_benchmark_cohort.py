"""Require one physical-host/compiler cohort for all published timing tables."""
import json

import site_targets as base


def validate(kernel):
    widgets = json.loads((base.SITE / 'generated/library-targets/matrix.json').read_text())
    workloads = json.loads((base.SITE / 'generated/json-text/matrix.json').read_text())
    examples = json.loads((base.SITE / 'generated/targets/matrix.json').read_text())
    revision = kernel['revision']
    assert kernel['virtualization'] == widgets['virtualization'] == 'none'
    assert widgets['hostname'] == 'alpha' and kernel['host'] == workloads['host'] == 'alpha · ' + widgets['host']
    assert revision == widgets['compiler']['revision'] == workloads['revision'] == examples['revision'], 'Mixed compiler cohorts'
    assert not kernel['compiler_dirty'] and not widgets['compiler']['dirty'] and not examples['compiler_dirty']
    assert set(examples['platforms']) == {'desktop'}
    assert examples['cpu'] == widgets['host']
    assert all(data['samples'] == 11 for data in (kernel, widgets, workloads, examples))
    for row in examples['platforms']['desktop']['examples'].values():
        assert all(value['host_gate']['accepted'] for value in row.values())
    for data in (widgets, workloads):
        for row in data['examples']:
            for cell in row['targets'].values():
                if cell['status'] == 'ok':
                    assert cell['measurement']['host_gate']['accepted']
