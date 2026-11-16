"""Require one physical-host/compiler cohort for all published timing tables."""
import html
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
    return (f'<p class="matrix-footnote">Measured on {html.escape(widgets["host"])} · '
            f'<a href="https://github.com/ziranlang/ziran/commit/{revision}">Compiler revision</a>. '
            'All published tables use this physical host and compiler revision. Times are in µs.</p>')
