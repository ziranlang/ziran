"""One symmetric outlier policy for all published benchmark timings."""
import math
import statistics

RULE = 'Tukey 1.5×IQR fences; linearly interpolated inclusive quartiles; both tails; raw samples retained'


def summarize_samples(samples):
    values = list(samples)
    if len(values) < 3 or not all(math.isfinite(x) and x > 0 for x in values):
        raise ValueError('Benchmark samples must contain at least three finite positive timings')
    ordered = sorted(values)

    def quantile(fraction):
        position = (len(ordered) - 1) * fraction
        lower = int(position)
        upper = min(lower + 1, len(ordered) - 1)
        return ordered[lower] + (ordered[upper] - ordered[lower]) * (position - lower)

    q1, q3 = quantile(.25), quantile(.75)
    spread = q3 - q1
    low, high = q1 - 1.5 * spread, q3 + 1.5 * spread
    excluded = [i for i, value in enumerate(values) if value < low or value > high]
    included = [value for i, value in enumerate(values) if i not in excluded]
    if len(included) < 3:
        raise ValueError('Too few benchmark samples remain after outlier filtering; collect another run')
    return {'rule': RULE, 'lower_ms': low, 'upper_ms': high,
            'excluded_indices': excluded, 'included_samples_ms': included,
            'median_ms': statistics.median(included)}
