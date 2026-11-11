# Portable ZX: unchanged-bundle comparison

The fresh desktop control is `baseline-desktop-all.json`, measured with the
original runtime at revision `8d44256676b5e125dfff96e0d551b5e0c1e8a1b7`.
The candidate is `repeated/summary.json`: three independent repetitions,
nine batches each. `comparison.json` computes the median of those medians.
All five bundles have identical SHA-256 hashes in control and candidate.
Each run verifies the printed output and every timed return-value checksum.

After applying the same symmetric 1.5×IQR filter independently to each set of
raw samples, loops improve from 24.251 to 1.698 microseconds (14.28 times
faster), records/calls from 8.622 to 0.273 (31.56 times), text/imports from
8.926 to 0.802 (11.13 times), arrays/slices from 16.642 to 1.707 (9.75 times),
and enums/branches from 13.326 to 1.117 (11.93 times). All exclusion indices,
filtered samples and fence bounds are recorded in `comparison.json`; original
raw evidence remains unchanged. The arrays improvement is below ten times.
These are warm entry measurements, not process-startup times or native-C parity
claims.

## Bottlenecks and general changes

- The old print path allocated and cleared roughly 256 KiB of format workspace
  for every printed line. Prepared immutable format plans retain only the
  needed steps and decoded bytes. Arguments are evaluated afresh, in order,
  into rooted storage; output uses the existing reusable buffer in chunks.
- Linux stack discovery called `pthread_getattr_np` on every entry; main-thread
  discovery reads `/proc/self/maps`. Bounds are now cached per thread while
  checking the current stack address and `RLIMIT_STACK` on every entry.
  Thread handoffs and changing limits are covered by an integration test.
- Repeated statement-close scans are replaced by indices prepared once from
  immutable checked IR.
- Existing prepared procedure execution now accepts primitive arrays/slices
  and flat records. Unsupported expressions and collection construction or
  assignment use the canonical evaluator inside the prepared control flow.
  Bounds, copy semantics, statement budgets and collection boundaries remain.

Intermediate raw measurements retain the causal steps. Print preparation alone
reduced the original loops measurement to about 13 microseconds; stack caching
reduced it to about 3.7; buffered output and prepared control flow reduced it
further. Individual noisy intermediate runs are not additive percentages.
The profiler experiments contain only a few samples and are diagnostic evidence,
not reliable statistical attribution.

## Validation and limits

Collection fixtures vary seeds, lengths, branch choices, copies, aliasing,
UTF-8 strings and empty/default arrays. The C integration test compares prepared
execution with the canonical evaluator at every tested statement budget.
Fixed expected values in the `.zi` correctness fixture are test assertions;
production dispatch contains no fixture names, expected answers or native
replacement for the portable workload.

The full local suite covers 524 tests; 26 VM tests also ran with ASan/UBSan.
Print tests include nested evaluation, signed/unsigned limits, long output and
embedded NUL bytes in portable ZX. The pre-existing C print path truncates NUL
strings, so the binary-byte oracle is explicitly VM-only.

Loading/startup and offline compilation are excluded. Formatting, null-sink
output, the host call boundary and normal VM budgets are included. Clocks and
CPU scheduling are uncontrolled. Tiny C record examples reduce to native
constant-return calls, so substantial relative ZX gains do not imply equal
nanosecond performance. `baseline-mobile.json` and `baseline-web.json` retain
the original USB Moto and private-browser controls; the website snapshot holds
the refreshed platform measurements.
