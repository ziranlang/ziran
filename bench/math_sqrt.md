# Exact portable square-root normalization

`std/math.zi` normalizes a finite positive input to a mantissa in `[1, 2)`.
After its existing 64-bit exponent steps, the residual exponent is in
`[0, 63]`. Six descending comparisons against `2^32`, `2^16`, `2^8`,
`2^4`, `2^2`, and `2^1` replace the old loop that constructed those powers
with 63 multiplications. Each selected reciprocal is an exactly representable
power of two. The normalized result is at least one, so replacing division by
that power with multiplication by its reciprocal cannot underflow, overflow,
or introduce rounding. The mantissa and exponent remain identical.

`Sqrt` keeps its six Newton iterations, neighboring-result selection and
exceptional-input paths. This is not a hardware-sqrt substitution: its existing
one-ULP accuracy contract, rather than correctly rounded libm behavior, remains
unchanged. No allocation, compiler lowering, runtime feature or host shim is
introduced. `Log`, which shares `MathSplit`, benefits from the same change.

## Local tests

Run from the canonical checkout, using an existing toolchain:

```sh
build-queue -- env -u DISPLAY -u WAYLAND_DISPLAY \
  build/bin/check-runner --bin-dir "$PWD/build/bin" --jobs 1 --filter math
```

Set `CC` to an existing managed compiler wrapper when compilation is assigned
to another machine. Both math shell tests honor it. `math_sqrt.sh` tests exact
agreement with the pre-change algorithm for one million deterministic IEEE
patterns, every normal exponent boundary, all subnormal powers and their
neighbors, signed zeros, NaNs, infinities and negative-domain inputs. Native
source and saved IR both run the full set. Portable source and saved IR run
the same bounded edge set and produce byte-identical bundles. The existing
`math_float64.sh` also checks all 21 elementary functions against libm and in
the portable VM. NaN tests compare classification, not payload bits.

## Repeatable microbenchmark

```sh
build-queue -- env -u DISPLAY -u WAYLAND_DISPLAY \
  sh bench/math_sqrt.sh "$PWD/build/bin/ziran"
```

The benchmark compiles the old algorithm from `tests/spec/math_sqrt_reference.zi`
and the current standard library together at `-O2`, without fast-math or LTO.
It first checks exact agreement on its inputs. Each sample makes 1,024,000
calls; a warmup precedes nine samples, alternating baseline/candidate order.
It covers squared-length-like values in `(0, 16]` and positive normal values
spread across binary64 exponents. Checksums keep the calls observable. Raw
nanoseconds per call and excluded sample indices are printed. Both algorithms
use the same symmetric 1.5-times-IQR rule with inclusive interpolated quartiles.

The November 11, 2026 local result is retained in
`results/math-sqrt-2026-11-11.json`: geometry median 32.04 to 11.44 ns/call
(2.80x); exponent-spread median 57.44 to 25.74 ns/call (2.23x). C objects were
compiled on physical Alpha with two workers and an 8 GiB managed limit;
timing ran locally on the Ryzen 9 9950X desktop. These are microbenchmarks,
not actual-room frame timings or proof of 60 fps. Parent-owned renderer
reference-frame and application integration validation remain separate.
