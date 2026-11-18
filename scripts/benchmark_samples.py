"""One symmetric outlier policy for all published benchmark timings."""
import json
import math
import os
from pathlib import Path
import statistics
import subprocess
import sys
import threading
import time

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


def cpu_ticks(cpu):
    """(busy, total) jiffies of one logical CPU from /proc/stat."""
    for line in Path('/proc/stat').read_text().splitlines():
        if line.startswith(f'cpu{cpu} '):
            fields = [int(value) for value in line.split()[1:9]]
            return sum(fields) - fields[3] - fields[4], sum(fields)
    raise RuntimeError(f'cpu{cpu} is missing from /proc/stat')


def busy_share(before, after):
    return (after[0] - before[0]) / max(1, after[1] - before[1])


GATE_METHOD = (' Host gate: every measured job runs pinned (inherited affinity, no wrapper) to one logical CPU of the quietest core, '
               'chosen again before each attempt once its other hyperthread has settled, and is repeated '
               'unless that CPU kept its clock within 3% of the job median (10th to 90th percentile), the median reached '
               '95% of the clock calibrated at start, and the hyperthread sibling was busy at most 15% of the job; '
               'Go runs with GOMAXPROCS=1 on that CPU. Rejected attempts are retained with their observations.')


class HostGate:
    """Pin each measured job to one logical CPU and keep it only when that CPU held a steady,
    full clock and its hyperthread sibling stayed mostly idle; otherwise run it again.

    Alpha's powersave/HWP clock moves between 0.8 and 4.3 GHz, run time scales inversely with
    it, and a busy sibling slows a core by about 30%
    (bench/results/records-native-frequency-alpha-2026-11-16.json). Rejected attempts stay in
    the receipt with their observations; accepted samples still follow the 1.5×IQR rule."""
    STEADY = 0.03     # 10th to 90th percentile clock within 3% of the job's median
    FLOOR = 0.95      # median clock at least 95% of the clock calibrated at start
    SIBLING = 0.15    # hyperthread sibling busy for at most 15% of the job
    ATTEMPTS = 8

    def __init__(self):
        self.cpus = sorted(os.sched_getaffinity(0))
        self.cores = {}
        for cpu in self.cpus:
            key = Path(f'/sys/devices/system/cpu/cpu{cpu}/topology/thread_siblings_list').read_text().strip()
            self.cores.setdefault(key, []).append(cpu)
        self.choose_core()
        spin = 'import time\nend = time.perf_counter() + 1.0\nwhile time.perf_counter() < end: pass'
        _, observed = self.observe(lambda: subprocess.run([sys.executable, '-c', spin], check=True), settle=0.3)
        self.reference_khz = observed['median_khz']
        if not self.reference_khz:
            raise RuntimeError(f'cpu{self.cpu} reports no scaling_cur_freq')

    def choose_core(self):
        """Pin to the quietest core over the last second, waiting up to a minute for its other
        hyperthread to settle, so a job does not start into another process's burst."""
        for _ in range(60):
            before = {cpu: cpu_ticks(cpu) for cpu in self.cpus}
            time.sleep(1.0)
            busy = {cpu: busy_share(before[cpu], cpu_ticks(cpu)) for cpu in self.cpus}
            core = min(self.cores.values(), key=lambda members: sum(busy[cpu] for cpu in members) / len(members))
            if len(core) < 2 or busy[core[1]] <= self.SIBLING / 3:
                break
        self.cpu, self.sibling = core[0], (core[1] if len(core) > 1 else None)
        # Measured children inherit this thread's CPU (no wrapper process adds to their
        # time); the clock poller thread runs on the other cores.
        self.free = set(self.cpus) - set(core) or set(self.cpus)
        os.sched_setaffinity(0, {self.cpu})

    def frequency(self):
        return int(Path(f'/sys/devices/system/cpu/cpu{self.cpu}/cpufreq/scaling_cur_freq').read_text())

    def observe(self, job, settle=0.2):
        readings, stop = [], threading.Event()

        def poll():
            os.sched_setaffinity(0, self.free)
            while not stop.wait(0.01):
                readings.append((time.monotonic(), self.frequency()))
        sibling_before = cpu_ticks(self.sibling) if self.sibling is not None else None
        poller = threading.Thread(target=poll, daemon=True)
        started = time.monotonic()
        poller.start()
        try:
            value = job()
        finally:
            stop.set()
            poller.join()
        # A short job (warmup included) may end inside the settle window: judge all its readings.
        clocks = sorted(khz for at, khz in readings if at - started >= settle)
        if len(clocks) < 3:
            clocks = sorted(khz for _, khz in readings)
        pick = (lambda share: clocks[min(len(clocks) - 1, int(share * len(clocks)))]) if clocks else (lambda share: None)
        return value, {'cpu': self.cpu, 'sibling': self.sibling, 'readings': len(clocks),
                       'median_khz': pick(0.5), 'p10_khz': pick(0.1), 'p90_khz': pick(0.9),
                       'sibling_busy': None if self.sibling is None else round(busy_share(sibling_before, cpu_ticks(self.sibling)), 4)}

    def rejection(self, observed):
        median = observed['median_khz']
        if not median or observed['readings'] < 3:
            return 'too few clock readings'
        if observed['p10_khz'] < (1 - self.STEADY) * median or observed['p90_khz'] > (1 + self.STEADY) * median:
            return 'clock not steady'
        if median < self.FLOOR * self.reference_khz:
            return 'clock below the calibrated sustained clock'
        if observed['sibling_busy'] is not None and observed['sibling_busy'] > self.SIBLING:
            return 'hyperthread sibling busy'
        return None

    @staticmethod
    def environment(env):
        # One pinned CPU: Go gets one scheduler instead of two time-sliced on it.
        return {**env, 'GOMAXPROCS': '1'}

    def describe(self):
        return {'cpu': self.cpu, 'sibling': self.sibling, 'reference_khz': self.reference_khz,
                'rules': {'steady': self.STEADY, 'floor': self.FLOOR, 'sibling_busy_max': self.SIBLING,
                          'attempts': self.ATTEMPTS}}

    def measure(self, job):
        attempts = []
        for _ in range(self.ATTEMPTS):
            measured, observed = self.observe(job)
            reason = self.rejection(observed)
            observed['accepted'] = reason is None
            attempts.append(observed)
            if reason is None:
                measured['host_gate'] = {**self.describe(), 'accepted': True, 'attempts': attempts}
                return measured
            observed['rejected_because'] = reason
            observed['measurement'] = measured
            print(f'host gate: attempt {len(attempts)} rejected on cpu{self.cpu}: {reason} '
                  f'(median {observed["median_khz"]} kHz, p10 {observed["p10_khz"]}, p90 {observed["p90_khz"]}, '
                  f'sibling {observed["sibling_busy"]})', file=sys.stderr, flush=True)
            self.choose_core()
        raise RuntimeError('Host gate rejected every attempt: ' + json.dumps(
            [{key: attempt[key] for key in ('rejected_because', 'median_khz', 'p10_khz', 'p90_khz', 'sibling_busy')}
             for attempt in attempts]) + f' (reference {self.reference_khz} kHz)')
