#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
PYTHONPATH="$repo/scripts" python3 - <<'PY'
from benchmark_samples import summarize_samples
result = summarize_samples([1, 10, 10, 10, 10, 10, 10, 10, 100])
assert result['excluded_indices'] == [0, 8]
assert result['included_samples_ms'] == [10] * 7 and result['median_ms'] == 10
raw = [10, 11, 12, 13, 14, 15, 16, 17, 200]
result = summarize_samples(raw)
assert raw[-1] == 200 and result['excluded_indices'] == [8]
assert result['median_ms'] == 13.5
assert summarize_samples([3, 2, 1])['excluded_indices'] == []
for invalid in ([1, 2], [1, 2, 0], [1, 2, float('inf')]):
    try:
        summarize_samples(invalid)
        raise AssertionError('Invalid timings were accepted')
    except ValueError:
        pass
print('Symmetric outlier filtering preserves raw samples, excludes both tails and handles small batches')
PY
